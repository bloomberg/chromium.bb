// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_sandbox_host_linux.h"

#include <fcntl.h>
#include <fontconfig/fontconfig.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <time.h>

#include <vector>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/linux_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/pickle.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/sandbox_methods_linux.h"
#include "content/common/unix_domain_socket_posix.h"
#include "skia/ext/SkFontHost_fontconfig_direct.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebFontInfo.h"
#include "webkit/glue/webkitplatformsupport_impl.h"

using WebKit::WebCString;
using WebKit::WebFontInfo;
using WebKit::WebUChar;

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

// BEWARE: code in this file run across *processes* (not just threads).

// This code runs in a child process
class SandboxIPCProcess  {
 public:
  // lifeline_fd: this is the read end of a pipe which the browser process
  //   holds the other end of. If the browser process dies, its descriptors are
  //   closed and we will noticed an EOF on the pipe. That's our signal to exit.
  // browser_socket: the browser's end of the sandbox IPC socketpair. From the
  //   point of view of the renderer, it's talking to the browser but this
  //   object actually services the requests.
  // sandbox_cmd: the path of the sandbox executable
  SandboxIPCProcess(int lifeline_fd, int browser_socket,
                    std::string sandbox_cmd)
      : lifeline_fd_(lifeline_fd),
        browser_socket_(browser_socket),
        font_config_(new FontConfigDirect()) {
    base::InjectiveMultimap multimap;
    multimap.push_back(base::InjectionArc(0, lifeline_fd, false));
    multimap.push_back(base::InjectionArc(0, browser_socket, false));

    base::CloseSuperfluousFds(multimap);

    if (!sandbox_cmd.empty()) {
      sandbox_cmd_.push_back(sandbox_cmd);
      sandbox_cmd_.push_back(base::kFindInodeSwitch);
    }
  }

  ~SandboxIPCProcess();

  void Run() {
    struct pollfd pfds[2];
    pfds[0].fd = lifeline_fd_;
    pfds[0].events = POLLIN;
    pfds[1].fd = browser_socket_;
    pfds[1].events = POLLIN;

    int failed_polls = 0;
    for (;;) {
      const int r = HANDLE_EINTR(poll(pfds, 2, -1));
      if (r < 1) {
        LOG(WARNING) << "poll errno:" << errno;
        if (failed_polls++ == 3) {
          LOG(FATAL) << "poll failing. Sandbox host aborting.";
          return;
        }
        continue;
      }

      failed_polls = 0;

      if (pfds[0].revents) {
        // our parent died so we should too.
        _exit(0);
      }

      if (pfds[1].revents) {
        HandleRequestFromRenderer(browser_socket_);
      }
    }
  }

 private:
  void EnsureWebKitInitialized();

  // ---------------------------------------------------------------------------
  // Requests from the renderer...

  void HandleRequestFromRenderer(int fd) {
    std::vector<int> fds;

    // A FontConfigIPC::METHOD_MATCH message could be kMaxFontFamilyLength
    // bytes long (this is the largest message type).
    // 128 bytes padding are necessary so recvmsg() does not return MSG_TRUNC
    // error for a maximum length message.
    char buf[FontConfigInterface::kMaxFontFamilyLength + 128];

    const ssize_t len = UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);
    if (len == -1) {
      // TODO: should send an error reply, or the sender might block forever.
      NOTREACHED()
          << "Sandbox host message is larger than kMaxFontFamilyLength";
      return;
    }
    if (fds.empty())
      return;

    Pickle pickle(buf, len);
    void* iter = NULL;

    int kind;
    if (!pickle.ReadInt(&iter, &kind))
      goto error;

    if (kind == FontConfigIPC::METHOD_MATCH) {
      HandleFontMatchRequest(fd, pickle, iter, fds);
    } else if (kind == FontConfigIPC::METHOD_OPEN) {
      HandleFontOpenRequest(fd, pickle, iter, fds);
    } else if (kind == LinuxSandbox::METHOD_GET_FONT_FAMILY_FOR_CHARS) {
      HandleGetFontFamilyForChars(fd, pickle, iter, fds);
    } else if (kind == LinuxSandbox::METHOD_LOCALTIME) {
      HandleLocaltime(fd, pickle, iter, fds);
    } else if (kind == LinuxSandbox::METHOD_GET_CHILD_WITH_INODE) {
      HandleGetChildWithInode(fd, pickle, iter, fds);
    } else if (kind == LinuxSandbox::METHOD_GET_STYLE_FOR_STRIKE) {
      HandleGetStyleForStrike(fd, pickle, iter, fds);
    } else if (kind == LinuxSandbox::METHOD_MAKE_SHARED_MEMORY_SEGMENT) {
      HandleMakeSharedMemorySegment(fd, pickle, iter, fds);
    } else if (kind == LinuxSandbox::METHOD_MATCH_WITH_FALLBACK) {
      HandleMatchWithFallback(fd, pickle, iter, fds);
    }

  error:
    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i) {
      close(*i);
    }
  }

  void HandleFontMatchRequest(int fd, const Pickle& pickle, void* iter,
                              std::vector<int>& fds) {
    bool filefaceid_valid;
    uint32_t filefaceid;

    if (!pickle.ReadBool(&iter, &filefaceid_valid))
      return;
    if (filefaceid_valid) {
      if (!pickle.ReadUInt32(&iter, &filefaceid))
        return;
    }
    bool is_bold, is_italic;
    if (!pickle.ReadBool(&iter, &is_bold) ||
        !pickle.ReadBool(&iter, &is_italic)) {
      return;
    }

    uint32_t characters_bytes;
    if (!pickle.ReadUInt32(&iter, &characters_bytes))
      return;
    const char* characters = NULL;
    if (characters_bytes > 0) {
      const uint32_t kMaxCharactersBytes = 1 << 10;
      if (characters_bytes % 2 != 0 ||  // We expect UTF-16.
          characters_bytes > kMaxCharactersBytes ||
          !pickle.ReadBytes(&iter, &characters, characters_bytes))
        return;
    }

    std::string family;
    if (!pickle.ReadString(&iter, &family))
      return;

    std::string result_family;
    unsigned result_filefaceid;
    const bool r = font_config_->Match(
        &result_family, &result_filefaceid, filefaceid_valid, filefaceid,
        family, characters, characters_bytes, &is_bold, &is_italic);

    Pickle reply;
    if (!r) {
      reply.WriteBool(false);
    } else {
      reply.WriteBool(true);
      reply.WriteUInt32(result_filefaceid);
      reply.WriteString(result_family);
      reply.WriteBool(is_bold);
      reply.WriteBool(is_italic);
    }
    SendRendererReply(fds, reply, -1);
  }

  void HandleFontOpenRequest(int fd, const Pickle& pickle, void* iter,
                             std::vector<int>& fds) {
    uint32_t filefaceid;
    if (!pickle.ReadUInt32(&iter, &filefaceid))
      return;
    const int result_fd = font_config_->Open(filefaceid);

    Pickle reply;
    if (result_fd == -1) {
      reply.WriteBool(false);
    } else {
      reply.WriteBool(true);
    }

    SendRendererReply(fds, reply, result_fd);

    if (result_fd >= 0)
      close(result_fd);
  }

  void HandleGetFontFamilyForChars(int fd, const Pickle& pickle, void* iter,
                                   std::vector<int>& fds) {
    // The other side of this call is
    // chrome/renderer/renderer_sandbox_support_linux.cc

    int num_chars;
    if (!pickle.ReadInt(&iter, &num_chars))
      return;

    // We don't want a corrupt renderer asking too much of us, it might
    // overflow later in the code.
    static const int kMaxChars = 4096;
    if (num_chars < 1 || num_chars > kMaxChars) {
      LOG(WARNING) << "HandleGetFontFamilyForChars: too many chars: "
                   << num_chars;
      return;
    }

    EnsureWebKitInitialized();
    scoped_array<WebUChar> chars(new WebUChar[num_chars]);

    for (int i = 0; i < num_chars; ++i) {
      uint32_t c;
      if (!pickle.ReadUInt32(&iter, &c)) {
        return;
      }

      chars[i] = c;
    }

    std::string preferred_locale;
    if (!pickle.ReadString(&iter, &preferred_locale))
      return;

    WebKit::WebFontFamily family;
    WebFontInfo::familyForChars(chars.get(),
                                num_chars,
                                preferred_locale.c_str(),
                                &family);

    Pickle reply;
    if (family.name.data()) {
      reply.WriteString(family.name.data());
    } else {
      reply.WriteString("");
    }
    reply.WriteBool(family.isBold);
    reply.WriteBool(family.isItalic);
    SendRendererReply(fds, reply, -1);
  }

  void HandleGetStyleForStrike(int fd, const Pickle& pickle, void* iter,
                               std::vector<int>& fds) {
    std::string family;
    int sizeAndStyle;

    if (!pickle.ReadString(&iter, &family) ||
        !pickle.ReadInt(&iter, &sizeAndStyle)) {
      return;
    }

    EnsureWebKitInitialized();
    WebKit::WebFontRenderStyle style;
    WebFontInfo::renderStyleForStrike(family.c_str(), sizeAndStyle, &style);

    Pickle reply;
    reply.WriteInt(style.useBitmaps);
    reply.WriteInt(style.useAutoHint);
    reply.WriteInt(style.useHinting);
    reply.WriteInt(style.hintStyle);
    reply.WriteInt(style.useAntiAlias);
    reply.WriteInt(style.useSubpixel);

    SendRendererReply(fds, reply, -1);
  }

  void HandleLocaltime(int fd, const Pickle& pickle, void* iter,
                       std::vector<int>& fds) {
    // The other side of this call is in zygote_main_linux.cc

    std::string time_string;
    if (!pickle.ReadString(&iter, &time_string) ||
        time_string.size() != sizeof(time_t)) {
      return;
    }

    time_t time;
    memcpy(&time, time_string.data(), sizeof(time));
    // We use localtime here because we need the tm_zone field to be filled
    // out. Since we are a single-threaded process, this is safe.
    const struct tm* expanded_time = localtime(&time);

    std::string result_string;
    const char* time_zone_string = "";
    if (expanded_time != NULL) {
      result_string = std::string(reinterpret_cast<const char*>(expanded_time),
                                  sizeof(struct tm));
      time_zone_string = expanded_time->tm_zone;
    }

    Pickle reply;
    reply.WriteString(result_string);
    reply.WriteString(time_zone_string);
    SendRendererReply(fds, reply, -1);
  }

  void HandleGetChildWithInode(int fd, const Pickle& pickle, void* iter,
                               std::vector<int>& fds) {
    // The other side of this call is in zygote_main_linux.cc
    if (sandbox_cmd_.empty()) {
      LOG(ERROR) << "Not in the sandbox, this should not be called";
      return;
    }

    uint64_t inode;
    if (!pickle.ReadUInt64(&iter, &inode))
      return;

    base::ProcessId pid = 0;
    std::string inode_output;

    std::vector<std::string> sandbox_cmd = sandbox_cmd_;
    sandbox_cmd.push_back(base::Int64ToString(inode));
    CommandLine get_inode_cmd(sandbox_cmd);
    if (base::GetAppOutput(get_inode_cmd, &inode_output))
      base::StringToInt(inode_output, &pid);

    if (!pid) {
      // Even though the pid is invalid, we still need to reply to the zygote
      // and not just return here.
      LOG(ERROR) << "Could not get pid";
    }

    Pickle reply;
    reply.WriteInt(pid);
    SendRendererReply(fds, reply, -1);
  }

  void HandleMakeSharedMemorySegment(int fd, const Pickle& pickle, void* iter,
                                     std::vector<int>& fds) {
    uint32_t shm_size;
    if (!pickle.ReadUInt32(&iter, &shm_size))
      return;
    int shm_fd = -1;
    base::SharedMemory shm;
    if (shm.CreateAnonymous(shm_size))
      shm_fd = shm.handle().fd;
    Pickle reply;
    SendRendererReply(fds, reply, shm_fd);
  }

  void HandleMatchWithFallback(int fd, const Pickle& pickle, void* iter,
                               std::vector<int>& fds) {
    // Unlike the other calls, for which we are an indirection in front of
    // WebKit or Skia, this call is always made via this sandbox helper
    // process. Therefore the fontconfig code goes in here directly.

    std::string face;
    bool is_bold, is_italic;
    uint32 charset;

    if (!pickle.ReadString(&iter, &face) ||
        face.empty() ||
        !pickle.ReadBool(&iter, &is_bold) ||
        !pickle.ReadBool(&iter, &is_italic) ||
        !pickle.ReadUInt32(&iter, &charset)) {
      return;
    }

    FcLangSet* langset = FcLangSetCreate();
    MSCharSetToFontconfig(langset, charset);

    FcPattern* pattern = FcPatternCreate();
    // TODO(agl): FC_FAMILy needs to change
    FcPatternAddString(pattern, FC_FAMILY, (FcChar8*) face.c_str());
    if (is_bold)
      FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
    if (is_italic)
      FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
    FcPatternAddLangSet(pattern, FC_LANG, langset);
    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcFontSet* font_set = FcFontSort(0, pattern, 0, 0, &result);
    int font_fd = -1;
    int good_enough_index = -1;
    bool good_enough_index_set = false;

    if (font_set) {
      for (int i = 0; i < font_set->nfont; ++i) {
        FcPattern* current = font_set->fonts[i];

        // Older versions of fontconfig have a bug where they cannot select
        // only scalable fonts so we have to manually filter the results.
        FcBool is_scalable;
        if (FcPatternGetBool(current, FC_SCALABLE, 0,
                             &is_scalable) != FcResultMatch ||
            !is_scalable) {
          continue;
        }

        FcChar8* c_filename;
        if (FcPatternGetString(current, FC_FILE, 0, &c_filename) !=
            FcResultMatch) {
          continue;
        }

        // We only want to return sfnt (TrueType) based fonts. We don't have a
        // very good way of detecting this so we'll filter based on the
        // filename.
        bool is_sfnt = false;
        static const char kSFNTExtensions[][5] = {
          ".ttf", ".otc", ".TTF", ".ttc", ""
        };
        const size_t filename_len = strlen(reinterpret_cast<char*>(c_filename));
        for (unsigned j = 0; ; j++) {
          if (kSFNTExtensions[j][0] == 0) {
            // None of the extensions matched.
            break;
          }
          const size_t ext_len = strlen(kSFNTExtensions[j]);
          if (filename_len > ext_len &&
              memcmp(c_filename + filename_len - ext_len,
                     kSFNTExtensions[j], ext_len) == 0) {
            is_sfnt = true;
            break;
          }
        }

        if (!is_sfnt)
          continue;

        // This font is good enough to pass muster, but we might be able to do
        // better with subsequent ones.
        if (!good_enough_index_set) {
          good_enough_index = i;
          good_enough_index_set = true;
        }

        FcValue matrix;
        bool have_matrix = FcPatternGet(current, FC_MATRIX, 0, &matrix) == 0;

        if (is_italic && have_matrix) {
          // we asked for an italic font, but fontconfig is giving us a
          // non-italic font with a transformation matrix.
          continue;
        }

        FcValue embolden;
        const bool have_embolden =
            FcPatternGet(current, FC_EMBOLDEN, 0, &embolden) == 0;

        if (is_bold && have_embolden) {
          // we asked for a bold font, but fontconfig gave us a non-bold font
          // and asked us to apply fake bolding.
          continue;
        }

        font_fd = open(reinterpret_cast<char*>(c_filename), O_RDONLY);
        if (font_fd >= 0)
          break;
      }
    }

    if (font_fd == -1 && good_enough_index_set) {
      // We didn't find a font that we liked, so we fallback to something
      // acceptable.
      FcPattern* current = font_set->fonts[good_enough_index];
      FcChar8* c_filename;
      FcPatternGetString(current, FC_FILE, 0, &c_filename);
      font_fd = open(reinterpret_cast<char*>(c_filename), O_RDONLY);
    }

    if (font_set)
      FcFontSetDestroy(font_set);
    FcPatternDestroy(pattern);

    Pickle reply;
    SendRendererReply(fds, reply, font_fd);

    if (font_fd >= 0) {
      if (HANDLE_EINTR(close(font_fd)) < 0)
        PLOG(ERROR) << "close";
    }
  }

  // MSCharSetToFontconfig translates a Microsoft charset identifier to a
  // fontconfig language set by appending to |langset|.
  static void MSCharSetToFontconfig(FcLangSet* langset, unsigned fdwCharSet) {
    // We have need to translate raw fdwCharSet values into terms that
    // fontconfig can understand. (See the description of fdwCharSet in the MSDN
    // documentation for CreateFont:
    // http://msdn.microsoft.com/en-us/library/dd183499(VS.85).aspx )
    //
    // Although the argument is /called/ 'charset', the actual values conflate
    // character sets (which are sets of Unicode code points) and character
    // encodings (which are algorithms for turning a series of bits into a
    // series of code points.) Sometimes the values will name a language,
    // sometimes they'll name an encoding. In the latter case I'm assuming that
    // they mean the set of code points in the domain of that encoding.
    //
    // fontconfig deals with ISO 639-1 language codes:
    //   http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
    //
    // So, for each of the documented fdwCharSet values I've had to take a
    // guess at the set of ISO 639-1 languages intended.

    switch (fdwCharSet) {
      case NPCharsetAnsi:
      // These values I don't really know what to do with, so I'm going to map
      // them to English also.
      case NPCharsetDefault:
      case NPCharsetMac:
      case NPCharsetOEM:
      case NPCharsetSymbol:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("en"));
        break;
      case NPCharsetBaltic:
        // The three baltic languages.
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("et"));
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("lv"));
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("lt"));
        break;
      // TODO(jungshik): Would we be better off mapping Big5 to zh-tw
      // and GB2312 to zh-cn? Fontconfig has 4 separate orthography
      // files (zh-{cn,tw,hk,mo}.
      case NPCharsetChineseBIG5:
      case NPCharsetGB2312:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("zh"));
        break;
      case NPCharsetEastEurope:
        // A scattering of eastern European languages.
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("pl"));
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("cs"));
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("sk"));
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("hu"));
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("hr"));
        break;
      case NPCharsetGreek:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("el"));
        break;
      case NPCharsetHangul:
      case NPCharsetJohab:
        // Korean
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ko"));
        break;
      case NPCharsetRussian:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ru"));
        break;
      case NPCharsetShiftJIS:
        // Japanese
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ja"));
        break;
      case NPCharsetTurkish:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("tr"));
        break;
      case NPCharsetVietnamese:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("vi"));
        break;
      case NPCharsetArabic:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ar"));
        break;
      case NPCharsetHebrew:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("he"));
        break;
      case NPCharsetThai:
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("th"));
        break;
      // default:
      // Don't add any languages in that case that we don't recognise the
      // constant.
    }
  }

  void SendRendererReply(const std::vector<int>& fds, const Pickle& reply,
                         int reply_fd) {
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    struct iovec iov = {const_cast<void*>(reply.data()), reply.size()};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control_buffer[CMSG_SPACE(sizeof(int))];

    if (reply_fd != -1) {
      struct stat st;
      if (fstat(reply_fd, &st) == 0 && S_ISDIR(st.st_mode)) {
        LOG(FATAL) << "Tried to send a directory descriptor over sandbox IPC";
        // We must never send directory descriptors to a sandboxed process
        // because they can use openat with ".." elements in the path in order
        // to escape the sandbox and reach the real filesystem.
      }

      struct cmsghdr *cmsg;
      msg.msg_control = control_buffer;
      msg.msg_controllen = sizeof(control_buffer);
      cmsg = CMSG_FIRSTHDR(&msg);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = CMSG_LEN(sizeof(int));
      memcpy(CMSG_DATA(cmsg), &reply_fd, sizeof(reply_fd));
      msg.msg_controllen = cmsg->cmsg_len;
    }

    if (HANDLE_EINTR(sendmsg(fds[0], &msg, MSG_DONTWAIT)) < 0)
      PLOG(ERROR) << "sendmsg";
  }

  // ---------------------------------------------------------------------------

  const int lifeline_fd_;
  const int browser_socket_;
  FontConfigDirect* const font_config_;
  std::vector<std::string> sandbox_cmd_;
  scoped_ptr<webkit_glue::WebKitPlatformSupportImpl> webkit_platform_support_;
};

SandboxIPCProcess::~SandboxIPCProcess() {
  if (webkit_platform_support_.get())
    WebKit::shutdown();
}

void SandboxIPCProcess::EnsureWebKitInitialized() {
  if (webkit_platform_support_.get())
    return;
  webkit_platform_support_.reset(new webkit_glue::WebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());
}

// -----------------------------------------------------------------------------

// Runs on the main thread at startup.
RenderSandboxHostLinux::RenderSandboxHostLinux()
    : initialized_(false),
      renderer_socket_(0),
      childs_lifeline_fd_(0),
      pid_(0) {
}

// static
RenderSandboxHostLinux* RenderSandboxHostLinux::GetInstance() {
  return Singleton<RenderSandboxHostLinux>::get();
}

void RenderSandboxHostLinux::Init(const std::string& sandbox_path) {
  DCHECK(!initialized_);
  initialized_ = true;

  int fds[2];
  // We use SOCK_SEQPACKET rather than SOCK_DGRAM to prevent the renderer from
  // sending datagrams to other sockets on the system. The sandbox may prevent
  // the renderer from calling socket() to create new sockets, but it'll still
  // inherit some sockets. With PF_UNIX+SOCK_DGRAM, it can call sendmsg to send
  // a datagram to any (abstract) socket on the same system. With
  // SOCK_SEQPACKET, this is prevented.
#if defined(OS_FREEBSD) || defined(OS_OPENBSD)
  // The BSDs often don't support SOCK_SEQPACKET yet, so fall back to
  // SOCK_DGRAM if necessary.
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) != 0)
    CHECK(socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) == 0);
#else
  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);
#endif

  renderer_socket_ = fds[0];
  const int browser_socket = fds[1];

  int pipefds[2];
  CHECK(0 == pipe(pipefds));
  const int child_lifeline_fd = pipefds[0];
  childs_lifeline_fd_ = pipefds[1];

  pid_ = fork();
  if (pid_ == 0) {
    SandboxIPCProcess handler(child_lifeline_fd, browser_socket, sandbox_path);
    handler.Run();
    _exit(0);
  }
}

RenderSandboxHostLinux::~RenderSandboxHostLinux() {
  if (initialized_) {
    if (HANDLE_EINTR(close(renderer_socket_)) < 0)
      PLOG(ERROR) << "close";
    if (HANDLE_EINTR(close(childs_lifeline_fd_)) < 0)
      PLOG(ERROR) << "close";
  }
}
