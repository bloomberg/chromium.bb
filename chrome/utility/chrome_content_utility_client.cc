// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "chrome/common/safe_browsing/zip_analyzer_results.h"
#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"
#include "chrome/utility/safe_json_parser_handler.h"
#include "chrome/utility/utility_message_handler.h"
#include "content/public/child/image_decoder_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_registry.h"
#include "content/public/utility/utility_thread.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "ipc/ipc_channel.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zlib/google/zip.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_CHROMEOS)
#include "ui/gfx/chromeos/codec/jpeg_codec_robust_slow.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/common/resource_usage_reporter.mojom.h"
#include "chrome/utility/profile_import_handler.h"
#include "net/proxy/mojo_proxy_resolver_factory_impl.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"
#endif

#if defined(OS_WIN)
#include "chrome/utility/font_cache_handler_win.h"
#include "chrome/utility/shell_handler_win.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/utility/extensions/extensions_handler.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#endif

#if defined(ENABLE_PRINT_PREVIEW) || defined(OS_WIN)
#include "chrome/utility/printing_handler.h"
#endif

#if defined(ENABLE_MDNS)
#include "chrome/utility/local_discovery/service_discovery_message_handler.h"
#endif

#if defined(OS_MACOSX) && defined(FULL_SAFE_BROWSING)
#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"
#endif

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

#if !defined(OS_ANDROID)
void CreateProxyResolverFactory(
    mojo::InterfaceRequest<net::interfaces::ProxyResolverFactory> request) {
  // MojoProxyResolverFactoryImpl is strongly bound to the Mojo message pipe it
  // is connected to. When that message pipe is closed, either explicitly on the
  // other end (in the browser process), or by a connection error, this object
  // will be destroyed.
  new net::MojoProxyResolverFactoryImpl(request.Pass());
}

class ResourceUsageReporterImpl : public ResourceUsageReporter {
 public:
  explicit ResourceUsageReporterImpl(
      mojo::InterfaceRequest<ResourceUsageReporter> req)
      : binding_(this, req.Pass()) {}
  ~ResourceUsageReporterImpl() override {}

 private:
  void GetUsageData(
      const mojo::Callback<void(ResourceUsageDataPtr)>& callback) override {
    ResourceUsageDataPtr data = ResourceUsageData::New();
    size_t total_heap_size = net::ProxyResolverV8::GetTotalHeapSize();
    if (total_heap_size) {
      data->reports_v8_stats = true;
      data->v8_bytes_allocated = total_heap_size;
      data->v8_bytes_used = net::ProxyResolverV8::GetUsedHeapSize();
    }
    callback.Run(data.Pass());
  }

  mojo::StrongBinding<ResourceUsageReporter> binding_;
};

void CreateResourceUsageReporter(
    mojo::InterfaceRequest<ResourceUsageReporter> request) {
  new ResourceUsageReporterImpl(request.Pass());
}
#endif  // OS_ANDROID

}  // namespace

int64_t ChromeContentUtilityClient::max_ipc_message_size_ =
    IPC::Channel::kMaximumMessageSize;

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : filter_messages_(false) {
#if !defined(OS_ANDROID)
  handlers_.push_back(new ProfileImportHandler());
#endif

#if defined(ENABLE_EXTENSIONS)
  handlers_.push_back(new extensions::ExtensionsHandler(this));
  handlers_.push_back(new image_writer::ImageWriterHandler());
#endif

#if defined(ENABLE_PRINT_PREVIEW) || defined(OS_WIN)
  handlers_.push_back(new PrintingHandler());
#endif

#if defined(ENABLE_MDNS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUtilityProcessEnableMDns)) {
    handlers_.push_back(new local_discovery::ServiceDiscoveryMessageHandler());
  }
#endif

#if defined(OS_WIN)
  handlers_.push_back(new ShellHandler());
  handlers_.push_back(new FontCacheHandler());
#endif

  handlers_.push_back(new SafeJsonParserHandler());
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() {
}

void ChromeContentUtilityClient::UtilityThreadStarted() {
#if defined(ENABLE_EXTENSIONS)
  extensions::UtilityHandler::UtilityThreadStarted();
#endif

  if (kMessageWhitelistSize > 0) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUtilityProcessRunningElevated)) {
      message_id_whitelist_.insert(kMessageWhitelist,
                                   kMessageWhitelist + kMessageWhitelistSize);
      filter_messages_ = true;
    }
  }
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  if (filter_messages_ && !ContainsKey(message_id_whitelist_, message.type()))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeContentUtilityClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_DecodeImage, OnDecodeImage)
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RobustJPEGDecodeImage,
                        OnRobustJPEGDecodeImage)
#endif  // defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_PatchFileBsdiff,
                        OnPatchFileBsdiff)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_PatchFileCourgette,
                        OnPatchFileCourgette)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_StartupPing, OnStartupPing)
#if defined(FULL_SAFE_BROWSING)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection,
                        OnAnalyzeZipFileForDownloadProtection)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeDmgFileForDownloadProtection,
                        OnAnalyzeDmgFileForDownloadProtection)
#endif  // defined(OS_MACOSX)
#endif  // defined(FULL_SAFE_BROWSING)
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CreateZipFile, OnCreateZipFile)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  for (Handlers::iterator it = handlers_.begin();
       !handled && it != handlers_.end(); ++it) {
    handled = (*it)->OnMessageReceived(message);
  }

  return handled;
}

void ChromeContentUtilityClient::RegisterMojoServices(
    content::ServiceRegistry* registry) {
#if !defined(OS_ANDROID)
  registry->AddService<net::interfaces::ProxyResolverFactory>(
      base::Bind(CreateProxyResolverFactory));
  registry->AddService<ResourceUsageReporter>(
      base::Bind(CreateResourceUsageReporter));
#endif
}

void ChromeContentUtilityClient::AddHandler(
    scoped_ptr<UtilityMessageHandler> handler) {
  handlers_.push_back(handler.Pass());
}

// static
void ChromeContentUtilityClient::PreSandboxStartup() {
#if defined(ENABLE_EXTENSIONS)
  extensions::ExtensionsHandler::PreSandboxStartup();
#endif

#if defined(ENABLE_MDNS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUtilityProcessEnableMDns)) {
    local_discovery::ServiceDiscoveryMessageHandler::PreSandboxStartup();
  }
#endif  // ENABLE_MDNS
}

// static
SkBitmap ChromeContentUtilityClient::DecodeImage(
    const std::vector<unsigned char>& encoded_data, bool shrink_to_fit) {
  SkBitmap decoded_image;
  if (encoded_data.empty())
    return decoded_image;

  decoded_image = content::DecodeImage(&encoded_data[0],
                                       gfx::Size(),
                                       encoded_data.size());

  int64_t struct_size = sizeof(ChromeUtilityHostMsg_DecodeImage_Succeeded);
  int64_t image_size = decoded_image.computeSize64();
  int halves = 0;
  while (struct_size + (image_size >> 2*halves) > max_ipc_message_size_)
    halves++;
  if (halves) {
    if (shrink_to_fit) {
      // If decoded image is too large for IPC message, shrink it by halves.
      // This prevents quality loss, and should never overshrink on displays
      // smaller than 3600x2400.
      // TODO (Issue 416916): Instead of shrinking, return via shared memory
      decoded_image = skia::ImageOperations::Resize(
          decoded_image, skia::ImageOperations::RESIZE_LANCZOS3,
          decoded_image.width() >> halves, decoded_image.height() >> halves);
    } else {
      // Image too big for IPC message, but caller didn't request resize;
      // pre-delete image so DecodeImageAndSend() will send an error.
      decoded_image.reset();
      LOG(ERROR) << "Decoded image too large for IPC message";
    }
  }

  return decoded_image;
}

// static
void ChromeContentUtilityClient::DecodeImageAndSend(
    const std::vector<unsigned char>& encoded_data,
    bool shrink_to_fit,
    int request_id) {
  SkBitmap decoded_image = DecodeImage(encoded_data, shrink_to_fit);

  if (decoded_image.empty()) {
    Send(new ChromeUtilityHostMsg_DecodeImage_Failed(request_id));
  } else {
    Send(new ChromeUtilityHostMsg_DecodeImage_Succeeded(decoded_image,
                                                        request_id));
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnDecodeImage(
    const std::vector<unsigned char>& encoded_data,
    bool shrink_to_fit,
    int request_id) {
  content::UtilityThread::Get()->EnsureBlinkInitialized();
  DecodeImageAndSend(encoded_data, shrink_to_fit, request_id);
}

#if defined(OS_CHROMEOS)
void ChromeContentUtilityClient::OnCreateZipFile(
    const base::FilePath& src_dir,
    const std::vector<base::FilePath>& src_relative_paths,
    const base::FileDescriptor& dest_fd) {
  // dest_fd should be closed in the function. See ipc/ipc_message_util.h for
  // details.
  base::ScopedFD fd_closer(dest_fd.fd);
  bool succeeded = true;

  // Check sanity of source relative paths. Reject if path is absolute or
  // contains any attempt to reference a parent directory ("../" tricks).
  for (std::vector<base::FilePath>::const_iterator iter =
           src_relative_paths.begin(); iter != src_relative_paths.end();
       ++iter) {
    if (iter->IsAbsolute() || iter->ReferencesParent()) {
      succeeded = false;
      break;
    }
  }

  if (succeeded)
    succeeded = zip::ZipFiles(src_dir, src_relative_paths, dest_fd.fd);

  if (succeeded)
    Send(new ChromeUtilityHostMsg_CreateZipFile_Succeeded());
  else
    Send(new ChromeUtilityHostMsg_CreateZipFile_Failed());
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
void ChromeContentUtilityClient::OnRobustJPEGDecodeImage(
    const std::vector<unsigned char>& encoded_data,
    int request_id) {
  // Our robust jpeg decoding is using IJG libjpeg.
  if (!encoded_data.empty()) {
    scoped_ptr<SkBitmap> decoded_image(gfx::JPEGCodecRobustSlow::Decode(
        &encoded_data[0], encoded_data.size()));
    if (!decoded_image.get() || decoded_image->empty()) {
      Send(new ChromeUtilityHostMsg_DecodeImage_Failed(request_id));
    } else {
      Send(new ChromeUtilityHostMsg_DecodeImage_Succeeded(*decoded_image,
                                                          request_id));
    }
  } else {
    Send(new ChromeUtilityHostMsg_DecodeImage_Failed(request_id));
  }
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_CHROMEOS)

void ChromeContentUtilityClient::OnPatchFileBsdiff(
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file) {
  if (input_file.empty() || patch_file.empty() || output_file.empty()) {
    Send(new ChromeUtilityHostMsg_PatchFile_Finished(-1));
  } else {
    const int patch_status = courgette::ApplyBinaryPatch(input_file,
                                                         patch_file,
                                                         output_file);
    Send(new ChromeUtilityHostMsg_PatchFile_Finished(patch_status));
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnPatchFileCourgette(
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file) {
  if (input_file.empty() || patch_file.empty() || output_file.empty()) {
    Send(new ChromeUtilityHostMsg_PatchFile_Finished(-1));
  } else {
    const int patch_status = courgette::ApplyEnsemblePatch(
        input_file.value().c_str(),
        patch_file.value().c_str(),
        output_file.value().c_str());
    Send(new ChromeUtilityHostMsg_PatchFile_Finished(patch_status));
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnStartupPing() {
  Send(new ChromeUtilityHostMsg_ProcessStarted);
  // Don't release the process, we assume further messages are on the way.
}

#if defined(FULL_SAFE_BROWSING)
void ChromeContentUtilityClient::OnAnalyzeZipFileForDownloadProtection(
    const IPC::PlatformFileForTransit& zip_file,
    const IPC::PlatformFileForTransit& temp_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::zip_analyzer::AnalyzeZipFile(
      IPC::PlatformFileForTransitToFile(zip_file),
      IPC::PlatformFileForTransitToFile(temp_file), &results);
  Send(new ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished(
      results));
  ReleaseProcessIfNeeded();
}

#if defined(OS_MACOSX)
void ChromeContentUtilityClient::OnAnalyzeDmgFileForDownloadProtection(
    const IPC::PlatformFileForTransit& dmg_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::dmg::AnalyzeDMGFile(
      IPC::PlatformFileForTransitToFile(dmg_file), &results);
  Send(new ChromeUtilityHostMsg_AnalyzeDmgFileForDownloadProtection_Finished(
      results));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_MACOSX)

#endif  // defined(FULL_SAFE_BROWSING)
