// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/examples/aura_demo/demo_screen.h"
#include "mojo/examples/aura_demo/window_tree_host_mojo.h"
#include "mojo/examples/launcher/launcher.mojom.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/default_activation_client.h"
#include "url/gurl.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define LAUNCHER_EXPORT __declspec(dllexport)
#else
#define CDECL
#define LAUNCHER_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace examples {

class MinimalInputEventFilter : public ui::internal::InputMethodDelegate,
                                public ui::EventHandler {
 public:
  explicit MinimalInputEventFilter(aura::Window* root)
      : root_(root),
        input_method_(ui::CreateInputMethod(this,
                                            gfx::kNullAcceleratedWidget)) {
    input_method_->Init(true);
    root_->AddPreTargetHandler(this);
    root_->SetProperty(aura::client::kRootWindowInputMethodKey,
                       input_method_.get());
  }

  virtual ~MinimalInputEventFilter() {
    root_->RemovePreTargetHandler(this);
    root_->SetProperty(aura::client::kRootWindowInputMethodKey,
                       static_cast<ui::InputMethod*>(NULL));
  }

 private:
  // ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    const ui::EventType type = event->type();
    if (type == ui::ET_TRANSLATED_KEY_PRESS ||
        type == ui::ET_TRANSLATED_KEY_RELEASE) {
      // The |event| is already handled by this object, change the type of the
      // event to ui::ET_KEY_* and pass it to the next filter.
      static_cast<ui::TranslatedKeyEvent*>(event)->ConvertToKeyEvent();
    } else {
      if (input_method_->DispatchKeyEvent(*event))
        event->StopPropagation();
    }
  }

  // ui::internal::InputMethodDelegate:
  virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& event) OVERRIDE {
    ui::TranslatedKeyEvent aura_event(event);
    ui::EventDispatchDetails details =
        root_->GetHost()->dispatcher()->OnEventFromSource(&aura_event);
    return aura_event.handled() || details.dispatcher_destroyed;
  }

  aura::Window* root_;
  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(MinimalInputEventFilter);
};

class LauncherWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit LauncherWindowTreeClient(aura::Window* window) : window_(window) {
    aura::client::SetWindowTreeClient(window_, this);
  }

  virtual ~LauncherWindowTreeClient() {
    aura::client::SetWindowTreeClient(window_, NULL);
  }

  // Overridden from aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    return window_;
  }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(LauncherWindowTreeClient);
};

// Called when the user has submitted a URL by pressing Enter.
class URLReceiver {
 public:
  virtual void OnURLEntered(const std::string& url_text) = 0;

 protected:
  virtual ~URLReceiver() {}
};

class LauncherController : public views::TextfieldController {
 public:
  explicit LauncherController(URLReceiver* url_receiver)
      : url_receiver_(url_receiver) {}

  void InitInWindow(aura::Window* parent) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.parent = parent;
    params.bounds = parent->bounds();
    params.can_activate = true;
    widget->Init(params);

    views::View* container = new views::View;
    container->set_background(
      views::Background::CreateSolidBackground(SK_ColorYELLOW));
    container->SetBorder(
      views::Border::CreateEmptyBorder(10, 10, 10, 10));
    container->SetLayoutManager(new views::FillLayout);
    widget->SetContentsView(container);

    views::Textfield* textfield = new views::Textfield;
    textfield->set_controller(this);
    container->AddChildView(textfield);
    textfield->RequestFocus();

    container->Layout();

    widget->Show();
  }

 private:
  // Overridden from views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE {
    if (key_event.key_code() == ui::VKEY_RETURN) {
      GURL url(sender->text());
      printf("Enter pressed with URL: %s\n", url.spec().c_str());
      url_receiver_->OnURLEntered(url.spec());
    }
    return false;
  }

  URLReceiver* url_receiver_;

  DISALLOW_COPY_AND_ASSIGN(LauncherController);
};

class LauncherImpl : public Application,
                     public Launcher,
                     public URLReceiver {
 public:
  explicit LauncherImpl(MojoHandle shell_handle)
      : Application(shell_handle),
        launcher_controller_(this),
        pending_show_(false) {
    screen_.reset(DemoScreen::Create());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());

    InterfacePipe<NativeViewport, AnyInterface> pipe;

    AllocationScope scope;
    shell()->Connect("mojo:mojo_native_viewport_service",
                     pipe.handle_to_peer.Pass());

    window_tree_host_.reset(new WindowTreeHostMojo(
        pipe.handle_to_self.Pass(), gfx::Rect(50, 50, 450, 60),
        base::Bind(&LauncherImpl::HostContextCreated, base::Unretained(this))));
  }

 private:
  // Overridden from Application:
  virtual void AcceptConnection(const mojo::String& url,
                                ScopedMessagePipeHandle handle) OVERRIDE {
    launcher_client_.reset(
        MakeScopedHandle(LauncherClientHandle(handle.release().value())).Pass(),
        this);
  }

  // Overridden from Launcher:
  virtual void Show() OVERRIDE {
    if (!window_tree_host_.get()) {
      pending_show_ = true;
      return;
    }
    window_tree_host_->Show();
  }
  virtual void Hide() OVERRIDE {
    window_tree_host_->Hide();
  }

  // Overridden from URLReceiver:
  virtual void OnURLEntered(const std::string& url_text) OVERRIDE {
    AllocationScope scope;
    launcher_client_->OnURLEntered(url_text);
  }

  void HostContextCreated() {
    window_tree_host_->InitHost();
    window_tree_host_->window()->SetBounds(gfx::Rect(450, 60));

    focus_client_.reset(new aura::test::TestFocusClient());
    aura::client::SetFocusClient(window_tree_host_->window(),
                                 focus_client_.get());
    new wm::DefaultActivationClient(window_tree_host_->window());
    capture_client_.reset(
        new aura::client::DefaultCaptureClient(window_tree_host_->window()));
    ime_filter_.reset(new MinimalInputEventFilter(window_tree_host_->window()));

    window_tree_client_.reset(
        new LauncherWindowTreeClient(window_tree_host_->window()));

    launcher_controller_.InitInWindow(window_tree_host_->window());

    if (pending_show_) {
      pending_show_ = false;
      Show();
    }
  }

  scoped_ptr<DemoScreen> screen_;
  scoped_ptr<LauncherWindowTreeClient> window_tree_client_;
  scoped_ptr<aura::client::FocusClient> focus_client_;
  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;
  scoped_ptr<ui::EventHandler> ime_filter_;

  LauncherController launcher_controller_;

  RemotePtr<LauncherClient> launcher_client_;
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;

  bool pending_show_;
};

}  // namespace examples
}  // namespace mojo

extern "C" LAUNCHER_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  CommandLine::Init(0, NULL);
  base::AtExitManager at_exit;
  base::i18n::InitializeICU();

  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);
  base::FilePath pak_file;
  pak_file = pak_dir.Append(FILE_PATH_LITERAL("ui_test.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);

  base::MessageLoop loop;
  mojo::GLES2Initializer gles2;

  // TODO(beng): This crashes in a DCHECK on X11 because this thread's
  //             MessageLoop is not of TYPE_UI. I think we need a way to build
  //             Aura that doesn't define platform-specific stuff.
  aura::Env::CreateInstance(true);
  mojo::examples::LauncherImpl launcher(shell_handle);
  loop.Run();

  return MOJO_RESULT_OK;
}
