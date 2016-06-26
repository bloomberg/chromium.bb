// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_content_renderer_client.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/test/test_mojo_service.mojom.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

#if defined(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_switches.h"
#endif

namespace content {

namespace {

// A test Mojo service which can be driven by browser tests for various reasons.
class TestMojoServiceImpl : public mojom::TestMojoService {
 public:
  explicit TestMojoServiceImpl(mojom::TestMojoServiceRequest request)
      : binding_(this, std::move(request)) {
    binding_.set_connection_error_handler(
        base::Bind(&TestMojoServiceImpl::OnConnectionError,
                   base::Unretained(this)));
  }

  ~TestMojoServiceImpl() override {}

 private:
  void OnConnectionError() { delete this; }

  // mojom::TestMojoService:
  void DoSomething(const DoSomethingCallback& callback) override {
    // Instead of responding normally, unbind the pipe, write some garbage,
    // and go away.
    const std::string kBadMessage = "This is definitely not a valid response!";
    mojo::ScopedMessagePipeHandle pipe = binding_.Unbind().PassMessagePipe();
    MojoResult rv = mojo::WriteMessageRaw(
        pipe.get(), kBadMessage.data(), kBadMessage.size(), nullptr, 0,
        MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(rv, MOJO_RESULT_OK);

    // Deletes this.
    OnConnectionError();
  }

  void DoTerminateProcess(const DoTerminateProcessCallback& callback) override {
    NOTREACHED();
  }

  void CreateFolder(const CreateFolderCallback& callback) override {
    NOTREACHED();
  }

  void GetRequestorName(const GetRequestorNameCallback& callback) override {
    callback.Run("Not implemented.");
  }

  mojo::Binding<mojom::TestMojoService> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoServiceImpl);
};

void CreateTestMojoService(mojom::TestMojoServiceRequest request) {
  // Owns itself.
  new TestMojoServiceImpl(std::move(request));
}

}  // namespace

ShellContentRendererClient::ShellContentRendererClient() {}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  web_cache_impl_.reset(new web_cache::WebCacheImpl());
}

void ShellContentRendererClient::RenderViewCreated(RenderView* render_view) {
  new ShellRenderViewObserver(render_view);
}

bool ShellContentRendererClient::IsPluginAllowedToUseCompositorAPI(
    const GURL& url) {
#if defined(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

bool ShellContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
#if defined(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

void ShellContentRendererClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::injectInternalsObject(context);
  }
}

void ShellContentRendererClient::ExposeInterfacesToBrowser(
    shell::InterfaceRegistry* interface_registry) {
  interface_registry->AddInterface<mojom::TestMojoService>(
      base::Bind(&CreateTestMojoService));
}

}  // namespace content
