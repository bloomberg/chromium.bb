// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process_host.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "content/common/child_process_info.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_paths.h"
#include "content/common/content_switches.h"
#include "ipc/ipc_logging.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#endif  // OS_LINUX

#if defined(OS_MACOSX)
namespace {

// Given |path| identifying a Mac-style child process executable path, adjusts
// it to correspond to |feature|. For a child process path such as
// ".../Chromium Helper.app/Contents/MacOS/Chromium Helper", the transformed
// path for feature "NP" would be
// ".../Chromium Helper NP.app/Contents/MacOS/Chromium Helper NP". The new
// path is returned.
FilePath TransformPathForFeature(const FilePath& path,
                                 const std::string& feature) {
  std::string basename = path.BaseName().value();

  FilePath macos_path = path.DirName();
  const char kMacOSName[] = "MacOS";
  DCHECK_EQ(kMacOSName, macos_path.BaseName().value());

  FilePath contents_path = macos_path.DirName();
  const char kContentsName[] = "Contents";
  DCHECK_EQ(kContentsName, contents_path.BaseName().value());

  FilePath helper_app_path = contents_path.DirName();
  const char kAppExtension[] = ".app";
  std::string basename_app = basename;
  basename_app.append(kAppExtension);
  DCHECK_EQ(basename_app, helper_app_path.BaseName().value());

  FilePath root_path = helper_app_path.DirName();

  std::string new_basename = basename;
  new_basename.append(1, ' ');
  new_basename.append(feature);
  std::string new_basename_app = new_basename;
  new_basename_app.append(kAppExtension);

  FilePath new_path = root_path.Append(new_basename_app)
                               .Append(kContentsName)
                               .Append(kMacOSName)
                               .Append(new_basename);

  return new_path;
}

}  // namespace
#endif  // OS_MACOSX

#if defined (OS_WIN)
// Types used in PreCacheFont
namespace {
typedef std::vector<string16> FontNameVector;
typedef std::map<int, FontNameVector> PidToFontNames;
}
#endif  // OS_WIN

ChildProcessHost::ChildProcessHost()
    : ALLOW_THIS_IN_INITIALIZER_LIST(listener_(this)),
      opening_channel_(false) {
}

ChildProcessHost::~ChildProcessHost() {
  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }
  listener_.Shutdown();
}

void ChildProcessHost::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  filters_.push_back(filter);

  if (channel_.get())
    filter->OnFilterAdded(channel_.get());
}

// static
FilePath ChildProcessHost::GetChildPath(int flags) {
  FilePath child_path;

  child_path = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kBrowserSubprocessPath);

#if defined(OS_LINUX)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  // When running under Valgrind, forking /proc/self/exe ends up forking the
  // Valgrind executable, which then crashes. However, it's almost safe to
  // assume that the updates won't happen while testing with Valgrind tools.
  if (child_path.empty() && flags & CHILD_ALLOW_SELF && !RunningOnValgrind())
    child_path = FilePath("/proc/self/exe");
#endif

  // On most platforms, the child executable is the same as the current
  // executable.
  if (child_path.empty())
    PathService::Get(content::CHILD_PROCESS_EXE, &child_path);

#if defined(OS_MACOSX)
  DCHECK(!(flags & CHILD_NO_PIE && flags & CHILD_ALLOW_HEAP_EXECUTION));

  // If needed, choose an executable with special flags set that inform the
  // kernel to enable or disable specific optional process-wide features.
  if (flags & CHILD_NO_PIE) {
    // "NP" is "No PIE". This results in Chromium Helper NP.app or
    // Google Chrome Helper NP.app.
    child_path = TransformPathForFeature(child_path, "NP");
  } else if (flags & CHILD_ALLOW_HEAP_EXECUTION) {
    // "EH" is "Executable Heap". A non-executable heap is only available to
    // 32-bit processes on Mac OS X 10.7. Most code can and should run with a
    // non-executable heap, but the "EH" feature is provided to allow code
    // intolerant of a non-executable heap to work properly on 10.7. This
    // results in Chromium Helper EH.app or Google Chrome Helper EH.app.
    child_path = TransformPathForFeature(child_path, "EH");
  }
#endif

  return child_path;
}

#if defined(OS_WIN)
ChildProcessHost::FontCache::CacheElement::CacheElement()
    : font_(NULL), dc_(NULL), ref_count_(0) {
}

ChildProcessHost::FontCache::CacheElement::~CacheElement() {
  if (font_) {
    DeleteObject(font_);
  }
  if (dc_) {
    DeleteDC(dc_);
  }
}

ChildProcessHost::FontCache::FontCache() {
}

ChildProcessHost::FontCache::~FontCache() {
}

// static
ChildProcessHost::FontCache* ChildProcessHost::FontCache::GetInstance() {
  return Singleton<ChildProcessHost::FontCache>::get();
}

void ChildProcessHost::FontCache::PreCacheFont(LOGFONT font, int process_id) {
  typedef std::map<string16, ChildProcessHost::FontCache::CacheElement>
          FontNameToElement;

  base::AutoLock lock(mutex_);

  // Fetch the font into memory.
  // No matter the font is cached or not, we load it to avoid GDI swapping out
  // that font file.
  HDC hdc = GetDC(NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  TEXTMETRIC tm;
  BOOL ret = GetTextMetrics(hdc, &tm);
  DCHECK(ret);

  string16 font_name = font.lfFaceName;
  int ref_count_inc = 1;
  FontNameVector::iterator it =
      std::find(process_id_font_map_[process_id].begin(),
                process_id_font_map_[process_id].end(),
                font_name);
  if (it == process_id_font_map_[process_id].end()) {
    // Requested font is new to cache.
    process_id_font_map_[process_id].push_back(font_name);
  } else {
    ref_count_inc = 0;
  }

  if (cache_[font_name].ref_count_ == 0) {  // Requested font is new to cache.
    cache_[font_name].ref_count_ = 1;
  } else {  // Requested font is already in cache, release old handles.
    DeleteObject(cache_[font_name].font_);
    DeleteDC(cache_[font_name].dc_);
  }
  cache_[font_name].font_ = font_handle;
  cache_[font_name].dc_ = hdc;
  cache_[font_name].ref_count_ += ref_count_inc;
}

void ChildProcessHost::FontCache::ReleaseCachedFonts(int process_id) {
  typedef std::map<string16, ChildProcessHost::FontCache::CacheElement>
          FontNameToElement;

  base::AutoLock lock(mutex_);

  PidToFontNames::iterator it;
  it = process_id_font_map_.find(process_id);
  if (it == process_id_font_map_.end()) {
    return;
  }

  for (FontNameVector::iterator i = it->second.begin(), e = it->second.end();
                                i != e; ++i) {
    FontNameToElement::iterator element;
    element = cache_.find(*i);
    if (element != cache_.end()) {
      --((*element).second.ref_count_);
    }
  }

  process_id_font_map_.erase(it);
  for (FontNameToElement::iterator i = cache_.begin(); i != cache_.end(); ) {
    if (i->second.ref_count_ == 0) {
      cache_.erase(i++);
    } else {
      ++i;
    }
  }
}

// static
void ChildProcessHost::PreCacheFont(LOGFONT font, int pid) {
  // If a child process is running in a sandbox, GetTextMetrics()
  // can sometimes fail. If a font has not been loaded
  // previously, GetTextMetrics() will try to load the font
  // from the font file. However, the sandboxed process does
  // not have permissions to access any font files and
  // the call fails. So we make the browser pre-load the
  // font for us by using a dummy call to GetTextMetrics of
  // the same font.
  // This means the browser process just loads the font into memory so that
  // when GDI attempt to query that font info in child process, it does not
  // need to load that file, hence no permission issues there.  Therefore,
  // when a font is asked to be cached, we always recreates the font object
  // to avoid the case that an in-cache font is swapped out by GDI.
  ChildProcessHost::FontCache::GetInstance()->PreCacheFont(font, pid);
}

// static
void ChildProcessHost::ReleaseCachedFonts(int pid) {
  // Release cached fonts that requested from a pid by decrementing the ref
  // count.  When ref count is zero, the handles are released.
  ChildProcessHost::FontCache::GetInstance()->ReleaseCachedFonts(pid);
}
#endif  // OS_WIN


bool ChildProcessHost::CreateChannel() {
  channel_id_ = ChildProcessInfo::GenerateRandomChannelID(this);
  channel_.reset(new IPC::Channel(
      channel_id_, IPC::Channel::MODE_SERVER, &listener_));
  if (!channel_->Connect())
    return false;

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_.get());

  // Make sure these messages get sent first.
#if defined(IPC_MESSAGE_LOG_ENABLED)
  bool enabled = IPC::Logging::GetInstance()->Enabled();
  Send(new ChildProcessMsg_SetIPCLoggingEnabled(enabled));
#endif

  Send(new ChildProcessMsg_AskBeforeShutdown());

  opening_channel_ = true;

  return true;
}

void ChildProcessHost::InstanceCreated() {
  Notify(content::NOTIFICATION_CHILD_INSTANCE_CREATED);
}

bool ChildProcessHost::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

void ChildProcessHost::OnChannelConnected(int32 peer_pid) {
}

void ChildProcessHost::OnChannelError() {
}

bool ChildProcessHost::Send(IPC::Message* message) {
  if (!channel_.get()) {
    delete message;
    return false;
  }
  return channel_->Send(message);
}

void ChildProcessHost::OnChildDied() {
  delete this;
}

void ChildProcessHost::OnChildDisconnected() {
  OnChildDied();
}

void ChildProcessHost::ShutdownStarted() {
}

void ChildProcessHost::Notify(int type) {
}

ChildProcessHost::ListenerHook::ListenerHook(ChildProcessHost* host)
    : host_(host) {
}

void ChildProcessHost::ListenerHook::Shutdown() {
  host_ = NULL;
}

bool ChildProcessHost::ListenerHook::OnMessageReceived(
    const IPC::Message& msg) {
  if (!host_)
    return true;

#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging* logger = IPC::Logging::GetInstance();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return true;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(msg);
#endif

  bool handled = false;
  for (size_t i = 0; i < host_->filters_.size(); ++i) {
    if (host_->filters_[i]->OnMessageReceived(msg)) {
      handled = true;
      break;
    }
  }

  if (!handled && msg.type() == ChildProcessHostMsg_ShutdownRequest::ID) {
    if (host_->CanShutdown())
      host_->Send(new ChildProcessMsg_Shutdown());
    handled = true;
  }

  if (!handled)
    handled = host_->OnMessageReceived(msg);

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(msg, host_->channel_id_);
#endif
  return handled;
}

void ChildProcessHost::ListenerHook::OnChannelConnected(int32 peer_pid) {
  if (!host_)
    return;
  host_->opening_channel_ = false;
  host_->OnChannelConnected(peer_pid);
  // Notify in the main loop of the connection.
  host_->Notify(content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED);

  for (size_t i = 0; i < host_->filters_.size(); ++i)
    host_->filters_[i]->OnChannelConnected(peer_pid);
}

void ChildProcessHost::ListenerHook::OnChannelError() {
  if (!host_)
    return;
  host_->opening_channel_ = false;
  host_->OnChannelError();

  for (size_t i = 0; i < host_->filters_.size(); ++i)
    host_->filters_[i]->OnChannelError();

  // This will delete host_, which will also destroy this!
  host_->OnChildDisconnected();
}

void ChildProcessHost::ForceShutdown() {
  Send(new ChildProcessMsg_Shutdown());
}
