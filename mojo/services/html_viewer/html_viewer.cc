// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/html_viewer/blink_platform_impl.h"
#include "mojo/services/html_viewer/html_document_view.h"
#include "mojo/services/html_viewer/webmediaplayer_factory.h"
#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if !defined(COMPONENT_BUILD)
#include "base/i18n/icu_util.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#endif

namespace mojo {

// Switches for html_viewer to be used with "--args-for". For example:
// --args-for='mojo:html_viewer --enable-mojo-media-renderer'

// Enable mojo::MediaRenderer in media pipeline instead of using the internal
// media::Renderer implementation.
const char kEnableMojoMediaRenderer[] = "--enable-mojo-media-renderer";

class HTMLViewer;

class ContentHandlerImpl : public InterfaceImpl<ContentHandler> {
 public:
  ContentHandlerImpl(scoped_refptr<base::MessageLoopProxy> compositor_thread,
                     WebMediaPlayerFactory* web_media_player_factory)
      : compositor_thread_(compositor_thread),
        web_media_player_factory_(web_media_player_factory) {}
  ~ContentHandlerImpl() override {}

 private:
  // Overridden from ContentHandler:
  void StartApplication(ShellPtr shell, URLResponsePtr response) override {
    new HTMLDocumentView(response.Pass(),
                         shell.Pass(),
                         compositor_thread_,
                         web_media_player_factory_);
  }

  scoped_refptr<base::MessageLoopProxy> compositor_thread_;
  WebMediaPlayerFactory* web_media_player_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class HTMLViewer : public ApplicationDelegate,
                   public InterfaceFactory<ContentHandler> {
 public:
  HTMLViewer() : compositor_thread_("compositor thread") {}

  ~HTMLViewer() override { blink::shutdown(); }

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    blink_platform_impl_.reset(new BlinkPlatformImpl(app));
    blink::initialize(blink_platform_impl_.get());
#if !defined(COMPONENT_BUILD)
    base::i18n::InitializeICU();

    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    CHECK(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
#endif

    bool enable_mojo_media_renderer = false;
    for (const auto& arg : app->args()) {
      if (arg == kEnableMojoMediaRenderer) {
        enable_mojo_media_renderer = true;
        break;
      }
    }

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
              InterfaceRequest<ContentHandler> request) override {
    BindToRequest(
        new ContentHandlerImpl(compositor_thread_.message_loop_proxy(),
                               web_media_player_factory_.get()),
        &request);
  }

  scoped_ptr<BlinkPlatformImpl> blink_platform_impl_;
  base::Thread compositor_thread_;
  scoped_ptr<WebMediaPlayerFactory> web_media_player_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::HTMLViewer);
  return runner.Run(shell_handle);
}
