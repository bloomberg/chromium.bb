// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/virtual_keyboard/virtual_keyboard_manager.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/screen.h"
#include "ui/views/ime/text_input_type_tracker.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/desktop.h"
#include "ui/aura/desktop_observer.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_window_ids.h"
#endif

namespace {

const int kDefaultKeyboardHeight = 300;
const int kKeyboardSlideDuration = 300;  // In milliseconds
const char kOnTextInputTypeChanged[] =
    "experimental.input.virtualKeyboard.onTextInputTypeChanged";

// The default position of the keyboard widget should be at the bottom,
// spanning the entire width of the desktop.
gfx::Rect GetKeyboardPosition(int height) {
  gfx::Rect area = gfx::Screen::GetMonitorAreaNearestPoint(gfx::Point());
  return gfx::Rect(area.x(), area.y() + area.height() - height,
                   area.width(), height);
}

}  // namespace

// TODO(sad): Is the default profile always going to be the one we want?

class KeyboardWidget
    : public views::Widget,
      public ui::AnimationDelegate,
      public TabContentsObserver,
      public ExtensionFunctionDispatcher::Delegate,
#if defined(OS_CHROMEOS)
      public chromeos::input_method::InputMethodManager::VirtualKeyboardObserver,
#endif
#if defined(USE_AURA)
      public aura::DesktopObserver,
#endif
      public content::NotificationObserver,
      public views::Widget::Observer,
      public views::TextInputTypeObserver {
 public:
  KeyboardWidget();
  virtual ~KeyboardWidget();

  // Show the keyboard for the target widget. The events from the keyboard will
  // be sent to |widget|.
  void ShowKeyboardForWidget(views::Widget* widget);

  // Updates the bounds to reflect the current screen/desktop bounds.
  void ResetBounds();

  // Overridden from views::Widget
  void Hide() OVERRIDE;

 private:
  // Sets the target widget, adds/removes Widget::Observer, reparents etc.
  void SetTarget(Widget* target);

  // Returns the widget of the active browser, or NULL if there is no such
  // widget.
  views::Widget* GetBrowserWidget();

  // Update layer opacity and transform with values in animation_.
  void UpdateForAnimation();

  // Overridden from views::Widget.
  virtual bool OnKeyEvent(const views::KeyEvent& event) OVERRIDE;

  // Overridden from ui::AnimationDelegate.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // Overridden from TabContentsObserver.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // Overridden from TextInputTypeObserver.
  virtual void TextInputTypeChanged(ui::TextInputType type,
                                    views::Widget *widget) OVERRIDE;

  // Overridden from ExtensionFunctionDispatcher::Delegate.
  virtual Browser* GetBrowser() OVERRIDE;
  virtual gfx::NativeView GetNativeViewOfHost() OVERRIDE;
  virtual TabContents* GetAssociatedTabContents() const OVERRIDE;

#if defined(OS_CHROMEOS)
  // Overridden from input_method::InputMethodManager::VirtualKeyboardObserver.
  virtual void VirtualKeyboardChanged(
      chromeos::input_method::InputMethodManager* manager,
      const chromeos::input_method::VirtualKeyboard& virtual_keyboard,
      const std::string& virtual_keyboard_layout);
#endif

#if defined(USE_AURA)
  // Overridden from aura::DesktopObserver.
  virtual void OnDesktopResized(const gfx::Size& new_size) OVERRIDE;
#endif

  // Overridden from NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(Widget* widget) OVERRIDE;
  virtual void OnWidgetVisibilityChanged(Widget* widget, bool visible) OVERRIDE;
  virtual void OnWidgetActivationChanged(Widget* widget, bool active) OVERRIDE;

  // The animation.
  scoped_ptr<ui::SlideAnimation> animation_;

  // Interpolated transform used during animation.
  scoped_ptr<ui::InterpolatedTransform> transform_;

  GURL keyboard_url_;

  // The DOM view to host the keyboard.
  DOMView* dom_view_;

  ExtensionFunctionDispatcher extension_dispatcher_;

  // The widget the events from the keyboard should be directed to.
  views::Widget* target_;

  // Height of the keyboard.
  int keyboard_height_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardWidget);
};

KeyboardWidget::KeyboardWidget()
    : views::Widget::Widget(),
      keyboard_url_(chrome::kChromeUIKeyboardURL),
      dom_view_(new DOMView),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_dispatcher_(ProfileManager::GetDefaultProfile(), this)),
      target_(NULL),
      keyboard_height_(kDefaultKeyboardHeight) {

  // Initialize the widget first.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.keep_on_top = true;
  params.transparent = true;
  params.bounds = GetKeyboardPosition(keyboard_height_);
  Init(params);
#if defined(USE_AURA)
  aura_shell::Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_MenusAndTooltipsContainer)->
      AddChild(GetNativeView());
#endif

  // Setup the DOM view to host the keyboard.
  Profile* profile = ProfileManager::GetDefaultProfile();
  dom_view_->Init(profile,
      SiteInstance::CreateSiteInstanceForURL(profile, keyboard_url_));
  dom_view_->LoadURL(keyboard_url_);
  dom_view_->SetVisible(true);
  SetContentsView(dom_view_);

  // Setup observer so the events from the keyboard can be handled.
  TabContentsObserver::Observe(dom_view_->dom_contents()->tab_contents());

  // Initialize the animation.
  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::LINEAR);
  animation_->SetSlideDuration(kKeyboardSlideDuration);

  views::TextInputTypeTracker::GetInstance()->AddTextInputTypeObserver(this);
  registrar_.Add(this,
                 chrome::NOTIFICATION_FOCUSED_EDITABLE_NODE_TOUCHED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_HIDE_KEYBOARD_INVOKED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SET_KEYBOARD_HEIGHT_INVOKED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  manager->AddVirtualKeyboardObserver(this);
#endif

#if defined(USE_AURA)
  aura::Desktop::GetInstance()->AddObserver(this);
#endif
}

KeyboardWidget::~KeyboardWidget() {
  if (target_)
    target_->RemoveObserver(this);
  views::TextInputTypeTracker::GetInstance()->RemoveTextInputTypeObserver(this);
#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  manager->RemoveVirtualKeyboardObserver(this);
#endif

#if defined(USE_AURA)
  aura::Desktop::GetInstance()->RemoveObserver(this);
#endif
  // TODO(sad): Do anything else?
}

void KeyboardWidget::ShowKeyboardForWidget(views::Widget* widget) {
  if (target_ == widget && IsVisible() && !animation_->is_animating())
    return;
  SetTarget(widget);

  transform_.reset(new ui::InterpolatedTranslation(
      gfx::Point(0, keyboard_height_), gfx::Point()));

  UpdateForAnimation();
  animation_->Show();

  Show();

  bool visible = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KEYBOARD_VISIBILITY_CHANGED,
      content::Source<KeyboardWidget>(this),
      content::Details<bool>(&visible));
}

void KeyboardWidget::ResetBounds() {
  SetBounds(GetKeyboardPosition(keyboard_height_));
}

void KeyboardWidget::Hide() {
  animation_->Hide();

  bool visible = false;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KEYBOARD_VISIBILITY_CHANGED,
      content::Source<KeyboardWidget>(this),
      content::Details<bool>(&visible));
}

void KeyboardWidget::SetTarget(views::Widget* target) {
  if (target_)
    target_->RemoveObserver(this);

  target_ = target;

  if (target_) {
    // TODO(sad): Make |target_| the parent widget.
    target_->AddObserver(this);
  } else if (IsVisible()) {
    Hide();
  }
}

views::Widget* KeyboardWidget::GetBrowserWidget() {
  Browser* browser = GetBrowser();
  if (!browser)
    return NULL;
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!view)
    return NULL;
  return view->GetWidget();
}

bool KeyboardWidget::OnKeyEvent(const views::KeyEvent& event) {
  return target_ ? target_->OnKeyEvent(event) : false;
}

void KeyboardWidget::UpdateForAnimation() {
  float t = static_cast<float>(animation_->GetCurrentValue());
  GetRootView()->SetTransform(transform_->Interpolate(t));
  if (GetRootView()->layer())
    GetRootView()->layer()->SetOpacity(t * t);
}

void KeyboardWidget::AnimationProgressed(const ui::Animation* animation) {
  UpdateForAnimation();
}

void KeyboardWidget::AnimationEnded(const ui::Animation* animation) {
  gfx::Rect keyboard_rect;
  if (animation_->GetCurrentValue() < 0.01)
    Widget::Hide();
  else
    keyboard_rect = GetWindowScreenBounds();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KEYBOARD_VISIBLE_BOUNDS_CHANGED,
      content::Source<KeyboardWidget>(this),
      content::Details<gfx::Rect>(&keyboard_rect));
}

bool KeyboardWidget::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(KeyboardWidget, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void KeyboardWidget::RenderViewGone(base::TerminationStatus status) {
  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    // Reload the keyboard if it crashes.
    dom_view_->LoadURL(keyboard_url_);
    dom_view_->SchedulePaint();
  }
}

void KeyboardWidget::OnRequest(const ExtensionHostMsg_Request_Params& request) {
  extension_dispatcher_.Dispatch(request,
      dom_view_->dom_contents()->tab_contents()->render_view_host());
}

void KeyboardWidget::TextInputTypeChanged(ui::TextInputType type,
                                          views::Widget *widget) {
  // Send onTextInputTypeChanged event to keyboard extension.
  ListValue args;
  switch (type) {
    case ui::TEXT_INPUT_TYPE_NONE: {
      args.Append(Value::CreateStringValue("none"));
      break;
    }
    case ui::TEXT_INPUT_TYPE_TEXT: {
      args.Append(Value::CreateStringValue("text"));
      break;
    }
    case ui::TEXT_INPUT_TYPE_PASSWORD: {
      args.Append(Value::CreateStringValue("password"));
      break;
    }
    case ui::TEXT_INPUT_TYPE_SEARCH: {
      args.Append(Value::CreateStringValue("search"));
      break;
    }
    case ui::TEXT_INPUT_TYPE_EMAIL: {
      args.Append(Value::CreateStringValue("email"));
      break;
    }
    case ui::TEXT_INPUT_TYPE_NUMBER: {
      args.Append(Value::CreateStringValue("number"));
      break;
    }
    case ui::TEXT_INPUT_TYPE_TELEPHONE: {
      args.Append(Value::CreateStringValue("tel"));
      break;
    }
    case ui::TEXT_INPUT_TYPE_URL: {
      args.Append(Value::CreateStringValue("url"));
      break;
    }
    default: {
      NOTREACHED();
      args.Append(Value::CreateStringValue("none"));
      break;
    }
  }

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  Profile* profile =
      Profile::FromBrowserContext(
          dom_view_->dom_contents()->tab_contents()->browser_context());
  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      kOnTextInputTypeChanged, json_args, NULL, GURL());

  if (type == ui::TEXT_INPUT_TYPE_NONE)
    Hide();
  else
    ShowKeyboardForWidget(widget);
}

Browser* KeyboardWidget::GetBrowser() {
  // TODO(sad): Find a better way. Perhaps just return NULL, and fix
  // SendKeyboardEventInputFunction::GetTopLevelWidget to somehow interact with
  // the WM to find the top level widget?
  return BrowserList::GetLastActive();
}

gfx::NativeView KeyboardWidget::GetNativeViewOfHost() {
  return dom_view_->native_view();
}

TabContents* KeyboardWidget::GetAssociatedTabContents() const {
  return dom_view_->dom_contents() ?
      dom_view_->dom_contents()->tab_contents() : NULL;
}

#if defined(OS_CHROMEOS)
void KeyboardWidget::VirtualKeyboardChanged(
    chromeos::input_method::InputMethodManager* manager,
    const chromeos::input_method::VirtualKeyboard& virtual_keyboard,
    const std::string& virtual_keyboard_layout) {
  const GURL& url = virtual_keyboard.GetURLForLayout(virtual_keyboard_layout);
  dom_view_->LoadURL(url);
  VLOG(1) << "VirtualKeyboardChanged: Switched to " << url.spec();
}
#endif

#if defined(USE_AURA)
void KeyboardWidget::OnDesktopResized(const gfx::Size& new_size) {
  ResetBounds();
}
#endif

void KeyboardWidget::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_FOCUSED_EDITABLE_NODE_TOUCHED: {
      // In case the keyboard hid itself and the focus is still in an editable
      // field, and the user touches the field, then we want to show the
      // keyboard again.
      views::Widget* widget = GetBrowserWidget();
      if (widget)
        ShowKeyboardForWidget(widget);
      break;
    }

    case chrome::NOTIFICATION_HIDE_KEYBOARD_INVOKED: {
      Hide();
      break;
    }

    case chrome::NOTIFICATION_SET_KEYBOARD_HEIGHT_INVOKED: {
      // The keyboard is resizing itself.

      // TODO(penghuang) Allow extension conrtol the virtual keyboard directly
      // instead of using Notification.
      int height = *content::Details<int>(details).ptr();
      if (height != keyboard_height_) {
        DCHECK_GE(height, 0) << "Keyboard height should not be negative.";

        int old_height = keyboard_height_;
        keyboard_height_ = height;
        gfx::Rect rect = GetWindowScreenBounds();
        rect.set_y(rect.y() + old_height - keyboard_height_);
        rect.set_height(keyboard_height_);
        SetBounds(rect);

        // TODO(sad): Notify the target widget that the size has changed so it
        // can update its display accordingly if it wanted to.
      }
      break;
    }

    case content::NOTIFICATION_APP_TERMINATING: {
      CloseNow();
      break;
    }

    default:
      NOTREACHED();
  }
}

void KeyboardWidget::OnWidgetClosing(Widget* widget) {
  if (target_ == widget)
    SetTarget(NULL);
}

void KeyboardWidget::OnWidgetVisibilityChanged(Widget* widget, bool visible) {
  if (target_ == widget && !visible)
    SetTarget(NULL);
}

void KeyboardWidget::OnWidgetActivationChanged(Widget* widget, bool active) {
  if (target_ == widget && !active)
    SetTarget(NULL);
}

VirtualKeyboardManager::VirtualKeyboardManager()
    : keyboard_(new KeyboardWidget()) {
  keyboard_->AddObserver(this);
}

VirtualKeyboardManager::~VirtualKeyboardManager() {
  DCHECK(!keyboard_);
}

void VirtualKeyboardManager::ShowKeyboardForWidget(views::Widget* widget) {
  keyboard_->ShowKeyboardForWidget(widget);
}

void VirtualKeyboardManager::Hide() {
  keyboard_->Hide();
}

views::Widget* VirtualKeyboardManager::keyboard() {
  return keyboard_;
}

void VirtualKeyboardManager::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(keyboard_, widget);
  keyboard_ = NULL;
}

// static
VirtualKeyboardManager* VirtualKeyboardManager::GetInstance() {
  return Singleton<VirtualKeyboardManager>::get();
}
