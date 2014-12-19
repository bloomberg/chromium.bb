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
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/content_handler/public/interfaces/content_handler.mojom.h"
#include "mojo/services/html_viewer/html_document.h"
#include "mojo/services/html_viewer/mojo_blink_platform_impl.h"
#include "mojo/services/html_viewer/webmediaplayer_factory.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if !defined(COMPONENT_BUILD)
#include "base/i18n/icu_util.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#endif

using mojo::ApplicationConnection;
using mojo::Array;
using mojo::ContentHandler;
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

class HTMLViewer;

class HTMLViewerApplication : public mojo::Application {
 public:
  HTMLViewerApplication(ShellPtr shell,
                        URLResponsePtr response,
                        scoped_refptr<base::MessageLoopProxy> compositor_thread,
                        WebMediaPlayerFactory* web_media_player_factory)
      : url_(response->url),
        shell_(shell.Pass()),
        initial_response_(response.Pass()),
        compositor_thread_(compositor_thread),
        web_media_player_factory_(web_media_player_factory) {
    shell_.set_client(this);
    ServiceProviderPtr service_provider;
    shell_->ConnectToApplication("mojo:network_service",
                                 GetProxy(&service_provider));
    ConnectToService(service_provider.get(), &network_service_);
  }

  void Initialize(Array<String> args) override {}

  void AcceptConnection(const String& requestor_url,
                        ServiceProviderPtr provider) override {
    if (initial_response_) {
      OnResponseReceived(URLLoaderPtr(), provider.Pass(),
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
                     base::Passed(&provider)));
    }
  }

 private:
  void OnResponseReceived(URLLoaderPtr loader,
                          ServiceProviderPtr provider,
                          URLResponsePtr response) {
    new HTMLDocument(provider.Pass(), response.Pass(), shell_.get(),
                     compositor_thread_, web_media_player_factory_);
  }

  String url_;
  ShellPtr shell_;
  mojo::NetworkServicePtr network_service_;
  URLResponsePtr initial_response_;
  scoped_refptr<base::MessageLoopProxy> compositor_thread_;
  WebMediaPlayerFactory* web_media_player_factory_;
};

class ContentHandlerImpl : public mojo::InterfaceImpl<ContentHandler> {
 public:
  ContentHandlerImpl(scoped_refptr<base::MessageLoopProxy> compositor_thread,
                     WebMediaPlayerFactory* web_media_player_factory)
      : compositor_thread_(compositor_thread),
        web_media_player_factory_(web_media_player_factory) {}
  ~ContentHandlerImpl() override {}

 private:
  // Overridden from ContentHandler:
  void StartApplication(ShellPtr shell, URLResponsePtr response) override {
    new HTMLViewerApplication(shell.Pass(), response.Pass(), compositor_thread_,
                              web_media_player_factory_);
  }

  scoped_refptr<base::MessageLoopProxy> compositor_thread_;
  WebMediaPlayerFactory* web_media_player_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class HTMLViewer : public mojo::ApplicationDelegate,
                   public mojo::InterfaceFactory<ContentHandler> {
 public:
  HTMLViewer() : compositor_thread_("compositor thread") {}

  ~HTMLViewer() override { blink::shutdown(); }

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    blink_platform_.reset(new MojoBlinkPlatformImpl(app));
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    gin::IsolateHolder::LoadV8Snapshot();
#endif
    blink::initialize(blink_platform_.get());
#if !defined(COMPONENT_BUILD)
    base::i18n::InitializeICU();

    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    CHECK(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
#endif

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

    bool enable_mojo_media_renderer =
        command_line->HasSwitch(kEnableMojoMediaRenderer);

    compositor_thread_.Start();
    web_media_player_factory_.reset(new WebMediaPlayerFactory(
        compositor_thread_.message_loop_proxy(), enable_mojo_media_renderer));
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
                               web_media_player_factory_.get()),
        &request);
  }

  scoped_ptr<MojoBlinkPlatformImpl> blink_platform_;
  base::Thread compositor_thread_;
  scoped_ptr<WebMediaPlayerFactory> web_media_player_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new HTMLViewer);
  return runner.Run(shell_handle);
}

}  // namespace html_viewer
