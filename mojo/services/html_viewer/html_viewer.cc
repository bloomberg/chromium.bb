// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "gin/public/isolate_holder.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/services/html_viewer/discardable_memory_allocator.h"
#include "mojo/services/html_viewer/html_document.h"
#include "mojo/services/html_viewer/mojo_blink_platform_impl.h"
#include "mojo/services/html_viewer/webmediaplayer_factory.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_connection.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_delegate.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/connect.h"
#include "third_party/mojo/src/mojo/public/cpp/application/interface_factory_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/mojo_services/src/content_handler/public/interfaces/content_handler.mojom.h"

#if !defined(COMPONENT_BUILD)
#include "base/i18n/icu_util.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#endif

using mojo::ApplicationConnection;
using mojo::Array;
using mojo::BindToRequest;
using mojo::ContentHandler;
using mojo::InterfaceRequest;
using mojo::ServiceProvider;
using mojo::ServiceProviderPtr;
using mojo::ShellPtr;
using mojo::String;
using mojo::URLLoaderPtr;
using mojo::URLResponsePtr;

namespace html_viewer {

// Switches for html_viewer to be used with "--args-for". For example:
// --args-for='mojo:html_viewer --enable-mojo-media-renderer'

// Enable MediaRenderer in media pipeline instead of using the internal
// media::Renderer implementation.
const char kEnableMojoMediaRenderer[] = "enable-mojo-media-renderer";

// Disables support for (unprefixed) Encrypted Media Extensions.
const char kDisableEncryptedMedia[] = "disable-encrypted-media";

// Prevents creation of any output surface.
const char kIsHeadless[] = "is-headless";

size_t kDesiredMaxMemory = 20 * 1024 * 1024;

class HTMLViewer;

class HTMLViewerApplication : public mojo::Application {
 public:
  HTMLViewerApplication(InterfaceRequest<Application> request,
                        URLResponsePtr response,
                        scoped_refptr<base::MessageLoopProxy> compositor_thread,
                        WebMediaPlayerFactory* web_media_player_factory,
                        bool is_headless)
      : url_(response->url),
        binding_(this, request.Pass()),
        initial_response_(response.Pass()),
        compositor_thread_(compositor_thread),
        web_media_player_factory_(web_media_player_factory),
        is_headless_(is_headless) {}

  void Initialize(ShellPtr shell,
                  Array<String> args,
                  const String& url) override {
    ServiceProviderPtr service_provider;
    shell_ = shell.Pass();
    shell_->ConnectToApplication("mojo:network_service",
                                 GetProxy(&service_provider), nullptr);
    ConnectToService(service_provider.get(), &network_service_);
  }

  void AcceptConnection(const String& requestor_url,
                        InterfaceRequest<ServiceProvider> services,
                        ServiceProviderPtr exposed_services,
                        const String& url) override {
    if (initial_response_) {
      OnResponseReceived(URLLoaderPtr(), services.Pass(),
                         initial_response_.Pass());
    } else {
      URLLoaderPtr loader;
      network_service_->CreateURLLoader(GetProxy(&loader));
      mojo::URLRequestPtr request(mojo::URLRequest::New());
      request->url = url_;
      request->auto_follow_redirects = true;

      // |loader| will be pass to the OnResponseReceived method through a
      // callback. Because order of evaluation is undefined, a reference to the
      // raw pointer is needed.
      mojo::URLLoader* raw_loader = loader.get();
      raw_loader->Start(
          request.Pass(),
          base::Bind(&HTMLViewerApplication::OnResponseReceived,
                     base::Unretained(this), base::Passed(&loader),
                     base::Passed(&services)));
    }
  }

  void RequestQuit() override {}

 private:
  void OnResponseReceived(URLLoaderPtr loader,
                          InterfaceRequest<ServiceProvider> services,
                          URLResponsePtr response) {
    new HTMLDocument(services.Pass(), response.Pass(), shell_.get(),
                     compositor_thread_, web_media_player_factory_,
                     is_headless_);
  }

  String url_;
  mojo::StrongBinding<mojo::Application> binding_;
  ShellPtr shell_;
  mojo::NetworkServicePtr network_service_;
  URLResponsePtr initial_response_;
  scoped_refptr<base::MessageLoopProxy> compositor_thread_;
  WebMediaPlayerFactory* web_media_player_factory_;
  bool is_headless_;
};

class ContentHandlerImpl : public mojo::InterfaceImpl<ContentHandler> {
 public:
  ContentHandlerImpl(scoped_refptr<base::MessageLoopProxy> compositor_thread,
                     WebMediaPlayerFactory* web_media_player_factory,
                     bool is_headless)
      : compositor_thread_(compositor_thread),
        web_media_player_factory_(web_media_player_factory),
        is_headless_(is_headless) {}
  ~ContentHandlerImpl() override {}

 private:
  // Overridden from ContentHandler:
  void StartApplication(InterfaceRequest<mojo::Application> request,
                        URLResponsePtr response) override {
    new HTMLViewerApplication(request.Pass(), response.Pass(),
                              compositor_thread_, web_media_player_factory_,
                              is_headless_);
  }

  scoped_refptr<base::MessageLoopProxy> compositor_thread_;
  WebMediaPlayerFactory* web_media_player_factory_;
  bool is_headless_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class HTMLViewer : public mojo::ApplicationDelegate,
                   public mojo::InterfaceFactory<ContentHandler> {
 public:
  HTMLViewer()
      : discardable_memory_allocator_(kDesiredMaxMemory),
        compositor_thread_("compositor thread") {}

  ~HTMLViewer() override { blink::shutdown(); }

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);

    blink_platform_.reset(new MojoBlinkPlatformImpl(app));
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    // Note: this requires file system access.
    gin::IsolateHolder::LoadV8Snapshot();
#endif
    blink::initialize(blink_platform_.get());
    base::i18n::InitializeICU();

    ui::RegisterPathProvider();

    base::CommandLine::StringVector command_line_args;
#if defined(OS_WIN)
    for (const auto& arg : app->args())
      command_line_args.push_back(base::UTF8ToUTF16(arg));
#elif defined(OS_POSIX)
    command_line_args = app->args();
#endif

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->InitFromArgv(command_line_args);

    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    logging::InitLogging(settings);
    // Display process ID, thread ID and timestamp in logs.
    logging::SetLogItems(true, true, true, false);

    if (command_line->HasSwitch(kDisableEncryptedMedia))
      blink::WebRuntimeFeatures::enableEncryptedMedia(false);

    is_headless_ = command_line->HasSwitch(kIsHeadless);
    if (!is_headless_) {
      // TODO(sky): consider putting this into the .so so that we don't need
      // file system access.
      base::FilePath ui_test_pak_path;
      CHECK(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
      ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
    }

    compositor_thread_.Start();
#if defined(OS_ANDROID)
    // TODO(sky): Get WebMediaPlayerFactory working on android.
    NOTIMPLEMENTED();
#else
    bool enable_mojo_media_renderer =
        command_line->HasSwitch(kEnableMojoMediaRenderer);

    web_media_player_factory_.reset(new WebMediaPlayerFactory(
        compositor_thread_.message_loop_proxy(), enable_mojo_media_renderer));
#endif
  }

  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // Overridden from InterfaceFactory<ContentHandler>
  void Create(ApplicationConnection* connection,
              mojo::InterfaceRequest<ContentHandler> request) override {
    BindToRequest(
        new ContentHandlerImpl(compositor_thread_.message_loop_proxy(),
                               web_media_player_factory_.get(), is_headless_),
        &request);
  }

  // Skia requires that we have one of these. Unlike the one used in chrome,
  // this doesn't use purgable shared memory. Instead, it tries to free the
  // oldest unlocked chunks on allocation.
  //
  // TODO(erg): In the long run, delete this allocator and get the real shared
  // memory based purging allocator working here.
  DiscardableMemoryAllocator discardable_memory_allocator_;

  scoped_ptr<MojoBlinkPlatformImpl> blink_platform_;
  base::Thread compositor_thread_;
  scoped_ptr<WebMediaPlayerFactory> web_media_player_factory_;
  // Set if the content will never be displayed.
  bool is_headless_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

}  // namespace html_viewer

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new html_viewer::HTMLViewer);
  return runner.Run(shell_handle);
}
