// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/keyboard/keyboard_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_touch.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/interpolated_transform.h"
#include "views/ime/text_input_client.h"
#include "views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"
#endif

namespace {

const int kDefaultKeyboardHeight = 300;
const int kKeyboardSlideDuration = 300;  // In milliseconds

PropertyAccessor<bool>* GetFocusedStateAccessor() {
  static PropertyAccessor<bool> state;
  return &state;
}

// Returns whether the keyboard visibility should be affected by this tab.
bool TabContentsCanAffectKeyboard(const TabContents* tab_contents) {
  // There may not be a browser, e.g. for the login window. But if there is
  // a browser, then |tab_contents| should be the active tab.
  Browser* browser = Browser::GetBrowserForController(
      &tab_contents->controller(), NULL);
  return browser == NULL ||
        (browser == BrowserList::GetLastActive() &&
         browser->GetSelectedTabContents() == tab_contents);
}

}  // namespace

// TODO(sad): Is the default profile always going to be the one we want?

KeyboardManager::KeyboardManager()
    : views::Widget::Widget(),
      dom_view_(new DOMView),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_dispatcher_(ProfileManager::GetDefaultProfile(), this)),
      target_(NULL),
      keyboard_height_(kDefaultKeyboardHeight) {

  // Initialize the widget first.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  Init(params);

  // Setup the DOM view to host the keyboard.
  Profile* profile = ProfileManager::GetDefaultProfile();
  GURL keyboard_url(chrome::kChromeUIKeyboardURL);
  dom_view_->Init(profile,
      SiteInstance::CreateSiteInstanceForURL(profile, keyboard_url));
  dom_view_->LoadURL(keyboard_url);
  dom_view_->SetVisible(true);
  SetContentsView(dom_view_);

  // Setup observer so the events from the keyboard can be handled.
  TabContentsObserver::Observe(dom_view_->tab_contents());

  // Initialize the animation.
  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::LINEAR);
  animation_->SetSlideDuration(kKeyboardSlideDuration);

  // Start listening to notifications to maintain the keyboard visibility, size
  // etc.
  registrar_.Add(this,
                 content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_HIDE_KEYBOARD_INVOKED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SET_KEYBOARD_HEIGHT_INVOKED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_EDITABLE_ELEMENT_TOUCHED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_APP_EXITING,
                 NotificationService::AllSources());

#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  manager->AddVirtualKeyboardObserver(this);
#endif
}

KeyboardManager::~KeyboardManager() {
#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  manager->RemoveVirtualKeyboardObserver(this);
#endif
  // TODO(sad): Do anything else?
}

void KeyboardManager::ShowKeyboardForWidget(views::Widget* widget) {
  target_ = widget;
  // TODO(sad): There needs to be some way to:
  //   - reset |target_| when it is destroyed
  //   - make |target_| the parent of this Widget

  gfx::Rect rect = target_->GetWindowScreenBounds();
  rect.set_y(rect.y() + rect.height() - keyboard_height_);
  rect.set_height(keyboard_height_);
  SetBounds(rect);

  transform_.reset(new ui::InterpolatedTranslation(
      gfx::Point(0, keyboard_height_), gfx::Point()));

  GetRootView()->SetTransform(
      transform_->Interpolate(animation_->GetCurrentValue()));
  animation_->Show();

  MoveToTop();
  Show();
}

void KeyboardManager::Hide() {
  animation_->Hide();
}

bool KeyboardManager::OnKeyEvent(const views::KeyEvent& event) {
  return target_ ? target_->OnKeyEvent(event) : false;
}

void KeyboardManager::AnimationProgressed(const ui::Animation* animation) {
  GetRootView()->SetTransform(
      transform_->Interpolate(animation_->GetCurrentValue()));
}

void KeyboardManager::AnimationEnded(const ui::Animation* animation) {
  if (animation_->GetCurrentValue() < 0.01)
    Widget::Hide();
}

void KeyboardManager::OnBrowserAdded(const Browser* browser) {
  browser->tabstrip_model()->AddObserver(this);
}

void KeyboardManager::OnBrowserRemoved(const Browser* browser) {
  browser->tabstrip_model()->RemoveObserver(this);
}

bool KeyboardManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(KeyboardManager, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void KeyboardManager::OnRequest(
    const ExtensionHostMsg_Request_Params& request) {
  extension_dispatcher_.Dispatch(request,
      dom_view_->tab_contents()->render_view_host());
}

void KeyboardManager::ActiveTabChanged(TabContentsWrapper* old_contents,
                                       TabContentsWrapper* new_contents,
                                       int index,
                                       bool user_gesture) {
  TabContents* contents = new_contents->tab_contents();
  if (!TabContentsCanAffectKeyboard(contents))
    return;

  // If the tab contents does not have the focus, then it should not affect the
  // keyboard visibility.
  views::View* view = static_cast<TabContentsViewTouch*>(contents->view());
  views::FocusManager* fmanager = view ? view->GetFocusManager() : NULL;
  if (!fmanager || !view->Contains(fmanager->GetFocusedView()))
    return;

  bool* editable = GetFocusedStateAccessor()->GetProperty(
      contents->property_bag());
  if (editable && *editable)
    ShowKeyboardForWidget(view->GetWidget());
  else
    Hide();
}

Browser* KeyboardManager::GetBrowser() {
  // TODO(sad): Find a better way. Perhaps just return NULL, and fix
  // SendKeyboardEventInputFunction::GetTopLevelWidget to somehow interact with
  // the WM to find the top level widget?
  return BrowserList::GetLastActive();
}

gfx::NativeView KeyboardManager::GetNativeViewOfHost() {
  return dom_view_->native_view();
}

TabContents* KeyboardManager::GetAssociatedTabContents() const {
  return dom_view_->tab_contents();
}

#if defined(OS_CHROMEOS)
void KeyboardManager::VirtualKeyboardChanged(
    chromeos::input_method::InputMethodManager* manager,
    const chromeos::input_method::VirtualKeyboard& virtual_keyboard,
    const std::string& virtual_keyboard_layout) {
  const GURL& url = virtual_keyboard.GetURLForLayout(virtual_keyboard_layout);
  dom_view_->LoadURL(url);
  VLOG(1) << "VirtualKeyboardChanged: Switched to " << url.spec();
}
#endif

void KeyboardManager::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      // When a navigation happens, we want to hide the keyboard if the focus is
      // in the web-page. Otherwise, the keyboard visibility should not change.
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      TabContents* tab_contents = controller->tab_contents();
      GetFocusedStateAccessor()->SetProperty(tab_contents->property_bag(),
                                             false);
      if (!TabContentsCanAffectKeyboard(tab_contents))
        break;

      TabContentsViewTouch* view =
          static_cast<TabContentsViewTouch*>(tab_contents->view());
      views::View* focused = view->GetFocusManager()->GetFocusedView();
      views::TextInputClient* input =
          focused ? focused->GetTextInputClient() : NULL;
      // Show the keyboard if the focused view supports text-input.
      if (input && input->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE)
        ShowKeyboardForWidget(focused->GetWidget());
      else
        Hide();
      break;
    }

    case content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE: {
      // If the focus in the page moved to an editable field, then the keyboard
      // should be visible, otherwise not.
      TabContents* tab_contents = Source<TabContents>(source).ptr();
      const bool editable = *Details<const bool>(details).ptr();
      GetFocusedStateAccessor()->SetProperty(tab_contents->property_bag(),
          editable);
      if (!TabContentsCanAffectKeyboard(tab_contents))
        break;

      if (editable) {
        TabContentsViewTouch* view =
            static_cast<TabContentsViewTouch*>(tab_contents->view());
        ShowKeyboardForWidget(view->GetWidget());
      } else {
        Hide();
      }

      break;
    }

    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      // Tab content was destroyed. Forget everything about it.
      GetFocusedStateAccessor()->DeleteProperty(
          Source<TabContents>(source).ptr()->property_bag());
      break;
    }

    case chrome::NOTIFICATION_HIDE_KEYBOARD_INVOKED: {
      // The keyboard is hiding itself.
      Browser* browser = BrowserList::GetLastActive();
      if (browser) {
        TabContents* tab_contents = browser->GetSelectedTabContents();
        if (tab_contents) {
          GetFocusedStateAccessor()->SetProperty(tab_contents->property_bag(),
              false);
        }
      }

      Hide();
      break;
    }

    case chrome::NOTIFICATION_SET_KEYBOARD_HEIGHT_INVOKED: {
      // The keyboard is resizing itself.

      // TODO(penghuang) Allow extension conrtol the virtual keyboard directly
      // instead of using Notification.
      int height = *Details<int>(details).ptr();
      if (height != keyboard_height_) {
        DCHECK_GE(height, 0) << "Keyboard height should not be negative.";

        keyboard_height_ = height;
        gfx::Size size = GetWindowScreenBounds().size();
        size.set_height(keyboard_height_);
        SetSize(size);

        // TODO(sad): Notify the target widget that the size has changed so it
        // can update its display accordingly if it wanted to.
      }
      break;
    }

    case chrome::NOTIFICATION_EDITABLE_ELEMENT_TOUCHED: {
      // In case the keyboard hid itself and the focus is still in an editable
      // field, and the user touches the field, then we want to show the
      // keyboard again.
      views::View* src = Source<views::View>(source).ptr();
      ShowKeyboardForWidget(src->GetWidget());
      break;
    }

    case content::NOTIFICATION_APP_EXITING: {
      // Ideally KeyboardManager would Close itself, but that ends up destroying
      // the singleton object, which causes a crash when all the singleton
      // objects are destroyed from the AtExitManager. So just destroy the
      // RootView here.
      DestroyRootView();
      break;
    }

    default:
      NOTREACHED();
  }
}

// static
KeyboardManager* KeyboardManager::GetInstance() {
  return Singleton<KeyboardManager>::get();
}
