// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/offscreen_tabs/offscreen_tabs_api.h"

#include <algorithm>
#include <vector>

#include "base/hash_tables.h"
#include "base/json/json_writer.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/lazy_instance.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/offscreen_tabs/offscreen_tabs_constants.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

using content::NavigationController;
using content::NotificationDetails;
using content::NotificationSource;
using content::WebContents;
using WebKit::WebInputEvent;

namespace keys = extensions::offscreen_tabs_constants;
namespace tabs_keys = extension_tabs_module_constants;
namespace events = extension_event_names;

namespace {

class ParentTab;

// This class is responsible for the life cycle of an offscreen tab.
class OffscreenTab : public content::NotificationObserver {
 public:
  OffscreenTab();
  virtual ~OffscreenTab();
  void Init(const GURL& url,
            const int width,
            const int height,
            Profile* profile,
            ParentTab* parent_tab);

  TabContentsWrapper* tab_contents() const {
    return tab_contents_wrapper_.get();
  }
  WebContents* web_contents() const {
    return tab_contents()->web_contents();
  }
  int GetID() const { return ExtensionTabUtil::GetTabId(web_contents()); }
  ParentTab* parent_tab() { return parent_tab_; }

  // Creates a representation of this OffscreenTab for use by the API methods.
  // Passes ownership to the caller.
  DictionaryValue* CreateValue() const;

  // Navigates the tab to the |url|.
  void NavigateToURL(const GURL& url) {
    web_contents()->GetController().LoadURL(
        url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
  }

  // Adjusts the tab's dimensions to the specified |width| and |height|.
  void SetSize(int width, int height) {
    // TODO(jstritar): this doesn't seem to work on ChromeOS.
    web_contents()->GetView()->SizeContents(gfx::Size(width, height));
  }

 private:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  scoped_ptr<TabContentsWrapper> tab_contents_wrapper_;
  ParentTab* parent_tab_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenTab);
};

typedef ScopedVector<OffscreenTab> OffscreenTabs;

// Holds info about a tab that has spawned at least one offscreen tab.
// Each ParentTab keeps track of its child offscreen tabs. The ParentTab is also
// responsible for killing its children when it navigates away or gets closed.
class ParentTab : public content::NotificationObserver {
 public:
  ParentTab();
  virtual ~ParentTab();
  void Init(WebContents* web_contents,
            const std::string& extension_id);

  TabContentsWrapper* tab_contents() { return tab_contents_wrapper_; }
  int GetID() { return ExtensionTabUtil::GetTabId(web_contents()); }
  WebContents* web_contents() {
    return tab_contents()->web_contents();
  }

  // Returns the offscreen tabs spawned by this tab.
  const OffscreenTabs& offscreen_tabs() { return offscreen_tabs_; }
  const std::string& extension_id() const { return extension_id_; }

  // Tab takes ownership of OffscreenTab.
  void AddOffscreenTab(OffscreenTab *tab);

  // Removes the offscreen |tab| and returns true if this parent has no more
  // offscreen tabs.
  bool RemoveOffscreenTab(OffscreenTab *tab);

 private:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  TabContentsWrapper* tab_contents_wrapper_;
  OffscreenTabs offscreen_tabs_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ParentTab);
};

// This map keeps track of all tabs that are happy parents of offscreen tabs.
class OffscreenTabMap {
 public:
  OffscreenTabMap();
  ~OffscreenTabMap();

  // Returns true if this map tracks |parent_tab|.
  bool ContainsTab(ParentTab* parent_tab);

  // Gets an offscreen tab by ID.
  bool GetOffscreenTab(const int offscreen_tab_id,
                       UIThreadExtensionFunction* function,
                       OffscreenTab** offscreen_tab,
                       std::string* error);

  // Gets a parent tab from its contents for the given extension id.
  // Returns NULL if no such tab exists.
  ParentTab* GetParentTab(WebContents* parent_contents,
                          const std::string& extension_id);

  // Creates a new offscreen tab and a mapping between the |parent_tab| and
  // the offscreen tab. Takes ownership of |parent_tab|, if it does not already
  // have a mapping for it.
  const OffscreenTab& CreateOffscreenTab(ParentTab* parent_tab,
                                         const GURL& url,
                                         const int width,
                                         const int height,
                                         const std::string& extension_id);

  // Removes the mapping between a parent tab and an offscreen tab.
  // May cause the OffscreenTab object associated with the parent to be deleted.
  bool RemoveOffscreenTab(const int offscreen_tab_id,
                          UIThreadExtensionFunction* function,
                          std::string* error);

  // Removes and deletes |parent_tab| and all it's offscreen tabs.
  void RemoveParentTab(ParentTab* parent_tab);

 private:
  typedef base::hash_map<int, linked_ptr<ParentTab> > TabMap;
  TabMap map_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenTabMap);
};

static base::LazyInstance<OffscreenTabMap> g_offscreen_tab_map =
    LAZY_INSTANCE_INITIALIZER;

// Gets the map of parent tabs to offscreen tabs.
OffscreenTabMap* GetMap() {
  return &g_offscreen_tab_map.Get();
}

// Gets the WebContents of the tab that instantiated the extension API call.
// Returns NULL if there was an error.
// Note that you can't create offscreen tabs from background pages, since they
// don't have an associated WebContents. The lifetime of offscreen tabs is tied
// to their creating tab, so requiring visible tabs as the parent helps prevent
// offscreen tab leaking.
WebContents* GetCurrentWebContents(UIThreadExtensionFunction* function,
                                   std::string* error) {
  WebContents* web_contents =
      function->dispatcher()->delegate()->GetAssociatedWebContents();
  if (web_contents)
    return web_contents;

  *error = keys::kCurrentTabNotFound;
  return NULL;
}

OffscreenTab::OffscreenTab() : parent_tab_(NULL) {}
OffscreenTab::~OffscreenTab() {}

void OffscreenTab::Init(const GURL& url,
                        const int width,
                        const int height,
                        Profile* profile,
                        ParentTab* parent_tab) {
  // Create the offscreen tab.
  WebContents* web_contents = WebContents::Create(
      profile, NULL, MSG_ROUTING_NONE, NULL, NULL);
  tab_contents_wrapper_.reset(new TabContentsWrapper(web_contents));

  // Setting the size starts the renderer.
  SetSize(width, height);
  NavigateToURL(url);

  parent_tab_ = parent_tab;

  // Register for tab notifications.
  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&(web_contents->GetController())));
}

DictionaryValue* OffscreenTab::CreateValue() const {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(
      tabs_keys::kIdKey, ExtensionTabUtil::GetTabId(web_contents()));
  result->SetString(tabs_keys::kUrlKey, web_contents()->GetURL().spec());
  result->SetInteger(tabs_keys::kWidthKey,
      web_contents()->GetView()->GetContainerSize().width());
  result->SetInteger(tabs_keys::kHeightKey,
      web_contents()->GetView()->GetContainerSize().height());
  return result;
}

void OffscreenTab::Observe(int type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  CHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);

  DictionaryValue* changed_properties = new DictionaryValue();
  changed_properties->SetString(
      tabs_keys::kUrlKey, web_contents()->GetURL().spec());

  ListValue args;
  args.Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(web_contents())));
  args.Append(changed_properties);
  args.Append(CreateValue());
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  // The event router only dispatches the event to renderers listening for the
  // event.
  Profile* profile = parent_tab_->tab_contents()->profile();
  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      events::kOnOffscreenTabUpdated, json_args, profile, GURL());
}

ParentTab::ParentTab() : tab_contents_wrapper_(NULL) {}
ParentTab::~ParentTab() {}

void ParentTab::Init(WebContents* web_contents,
                     const std::string& extension_id) {
  CHECK(web_contents);

  extension_id_ = extension_id;
  tab_contents_wrapper_ =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents);

  CHECK(tab_contents_wrapper_);

  // Register for tab notifications.
  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&(web_contents->GetController())));
  registrar_.Add(
      this,
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(web_contents));
}

void ParentTab::AddOffscreenTab(OffscreenTab *offscreen_tab) {
  offscreen_tabs_.push_back(offscreen_tab);
}

bool ParentTab::RemoveOffscreenTab(OffscreenTab *offscreen_tab) {
  OffscreenTabs::iterator it_tab = std::find(
      offscreen_tabs_.begin(), offscreen_tabs_.end(), offscreen_tab);
  offscreen_tabs_.erase(it_tab);
  return offscreen_tabs_.empty();
}

void ParentTab::Observe(int type,
                        const NotificationSource& source,
                        const NotificationDetails& details) {
  CHECK(type == content::NOTIFICATION_NAV_ENTRY_COMMITTED ||
        type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  GetMap()->RemoveParentTab(this);
}

OffscreenTabMap::OffscreenTabMap() {}
OffscreenTabMap::~OffscreenTabMap() {}

bool OffscreenTabMap::ContainsTab(ParentTab* parent_tab) {
  return map_.find(parent_tab->GetID()) != map_.end();
}

bool OffscreenTabMap::GetOffscreenTab(const int offscreen_tab_id,
                                      UIThreadExtensionFunction* function,
                                      OffscreenTab** offscreen_tab,
                                      std::string* error) {
  // Ensure that the current tab is the parent of the offscreen tab.
  WebContents* web_contents = GetCurrentWebContents(function, error);
  if (!web_contents)
    return false;

  ParentTab* parent_tab = GetMap()->GetParentTab(
      web_contents, function->extension_id());
  if (parent_tab) {
    const OffscreenTabs& tabs = parent_tab->offscreen_tabs();
    for (OffscreenTabs::const_iterator i = tabs.begin(); i != tabs.end(); ++i) {
      if ((*i)->GetID() == offscreen_tab_id) {
        *offscreen_tab = *i;
        return true;
      }
    }
  }

  *error = ExtensionErrorUtils::FormatErrorMessage(
      keys::kOffscreenTabNotFoundError, base::IntToString(offscreen_tab_id));
  return false;
}

ParentTab* OffscreenTabMap::GetParentTab(WebContents* parent_contents,
                                         const std::string& extension_id) {
  CHECK(parent_contents);

  int parent_tab_id = ExtensionTabUtil::GetTabId(parent_contents);
  if (map_.find(parent_tab_id) == map_.end())
    return NULL;

  return map_[parent_tab_id].get();
}

const OffscreenTab& OffscreenTabMap::CreateOffscreenTab(
    ParentTab* parent_tab,
    const GURL& url,
    const int width,
    const int height,
    const std::string& ext_id) {
  CHECK(parent_tab);

  // Assume ownership of |parent_tab| if we haven't already.
  if (!ContainsTab(parent_tab)) {
    map_[ExtensionTabUtil::GetTabId(parent_tab->web_contents())].reset(
        parent_tab);
  }

  OffscreenTab* offscreen_tab = new OffscreenTab();
  offscreen_tab->Init(
      url, width, height, parent_tab->tab_contents()->profile(), parent_tab);
  parent_tab->AddOffscreenTab(offscreen_tab);

  return *offscreen_tab;
}

bool OffscreenTabMap::RemoveOffscreenTab(
    const int offscreen_tab_id,
    UIThreadExtensionFunction* function,
    std::string* error) {
  OffscreenTab* offscreen_tab = NULL;
  if (!GetOffscreenTab(offscreen_tab_id, function, &offscreen_tab, error))
    return false;

  // Tell the parent tab to remove the offscreen tab, and then remove the
  // parent tab if there are no more children.
  ParentTab* parent_tab = offscreen_tab->parent_tab();
  if (parent_tab->RemoveOffscreenTab(offscreen_tab))
    RemoveParentTab(parent_tab);

  return true;
}

void OffscreenTabMap::RemoveParentTab(ParentTab* parent_tab) {
  CHECK(parent_tab);
  CHECK(ContainsTab(parent_tab));

  map_.erase(parent_tab->GetID());
}

bool CopyModifiers(const DictionaryValue* js_event,
                   WebInputEvent* event) {
  bool alt_key = false;
  if (js_event->HasKey(keys::kEventAltKeyKey)) {
    if (!js_event->GetBoolean(keys::kEventAltKeyKey, &alt_key))
      return false;
  }
  if (alt_key)
    event->modifiers |= WebInputEvent::AltKey;

  bool ctrl_key = false;
  if (js_event->HasKey(keys::kEventCtrlKeyKey)) {
    if (!js_event->GetBoolean(keys::kEventCtrlKeyKey, &ctrl_key))
      return false;
  }
  if (ctrl_key)
    event->modifiers |= WebInputEvent::ControlKey;

  bool meta_key = false;
  if (js_event->HasKey(keys::kEventMetaKeyKey)) {
    if (!js_event->GetBoolean(keys::kEventMetaKeyKey, &meta_key))
      return false;
  }
  if (meta_key)
    event->modifiers |= WebInputEvent::MetaKey;

  bool shift_key;
  if (js_event->HasKey(keys::kEventShiftKeyKey)) {
    if (!js_event->GetBoolean(keys::kEventShiftKeyKey, &shift_key))
      return false;
  }
  if (shift_key)
    event->modifiers |= WebInputEvent::ShiftKey;
  return true;
}

}  // namespace

CreateOffscreenTabFunction::CreateOffscreenTabFunction() {}
CreateOffscreenTabFunction::~CreateOffscreenTabFunction() {}

bool CreateOffscreenTabFunction::RunImpl() {
  DictionaryValue* create_props;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &create_props));

  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(create_props->GetString(
      tabs_keys::kUrlKey, &url_string));

  GURL url = ExtensionTabUtil::ResolvePossiblyRelativeURL(
      url_string, GetExtension());
  if (!url.is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        tabs_keys::kInvalidUrlError, url_string);
    return false;
  }

  if (ExtensionTabUtil::IsCrashURL(url)) {
    error_ = tabs_keys::kNoCrashBrowserError;
    return false;
  }

  gfx::Rect window_bounds;
  Browser* browser = GetCurrentBrowser();
  if (!browser) {
    error_ = tabs_keys::kNoCurrentWindowError;
    return false;
  }

  WindowSizer::GetBrowserWindowBounds(
      std::string(), gfx::Rect(), browser, &window_bounds);

  int width = window_bounds.width();
  if (create_props->HasKey(tabs_keys::kWidthKey))
    EXTENSION_FUNCTION_VALIDATE(
        create_props->GetInteger(tabs_keys::kWidthKey, &width));

  int height = window_bounds.height();
  if (create_props->HasKey(tabs_keys::kHeightKey))
    EXTENSION_FUNCTION_VALIDATE(
        create_props->GetInteger(tabs_keys::kHeightKey, &height));


  // Add the offscreen tab to the map so we don't lose track of it.
  WebContents* web_contents = GetCurrentWebContents(this, &error_);
  if (!web_contents)
    return false;

  ParentTab* parent_tab = GetMap()->GetParentTab(web_contents, extension_id());
  if (!parent_tab) {
    // Ownership is passed to the OffscreenMap in CreateOffscreenTab.
    parent_tab = new ParentTab();
    parent_tab->Init(web_contents, extension_id());
  }

  const OffscreenTab& offscreen_tab = GetMap()->CreateOffscreenTab(
      parent_tab, url, width, height, extension_id());

  // TODO(alexbost): Maybe the callback is called too soon. It should probably
  // be called once we have navigated to the url.
  if (has_callback()) {
    result_.reset(offscreen_tab.CreateValue());
    SendResponse(true);
  }

  return true;
}

GetOffscreenTabFunction::GetOffscreenTabFunction() {}
GetOffscreenTabFunction::~GetOffscreenTabFunction() {}

bool GetOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(
          offscreen_tab_id, this, &offscreen_tab, &error_)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kOffscreenTabNotFoundError, base::IntToString(offscreen_tab_id));
    return false;
  }

  result_.reset(offscreen_tab->CreateValue());
  return true;
}

GetAllOffscreenTabFunction::GetAllOffscreenTabFunction() {}
GetAllOffscreenTabFunction::~GetAllOffscreenTabFunction() {}

bool GetAllOffscreenTabFunction::RunImpl() {
  WebContents* web_contents = GetCurrentWebContents(this, &error_);
  if (!web_contents)
    return NULL;

  ParentTab* parent_tab = GetMap()->GetParentTab(web_contents, extension_id());
  ListValue* tab_list = new ListValue();
  if (parent_tab) {
    for (OffscreenTabs::const_iterator i = parent_tab->offscreen_tabs().begin();
         i != parent_tab->offscreen_tabs().end(); ++i)
      tab_list->Append((*i)->CreateValue());
  }

  result_.reset(tab_list);
  return true;
}

RemoveOffscreenTabFunction::RemoveOffscreenTabFunction() {}
RemoveOffscreenTabFunction::~RemoveOffscreenTabFunction() {}

bool RemoveOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(
          offscreen_tab_id, this, &offscreen_tab, &error_))
    return false;

  if (!GetMap()->RemoveOffscreenTab(offscreen_tab_id, this, &error_))
    return false;

  return true;
}

SendKeyboardEventOffscreenTabFunction::
    SendKeyboardEventOffscreenTabFunction() {}
SendKeyboardEventOffscreenTabFunction::
    ~SendKeyboardEventOffscreenTabFunction() {}

bool SendKeyboardEventOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(
          offscreen_tab_id, this, &offscreen_tab, &error_))
    return false;

  // JavaScript KeyboardEvent.
  DictionaryValue* js_keyboard_event = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &js_keyboard_event));

  NativeWebKeyboardEvent keyboard_event;

  std::string type;
  if (js_keyboard_event->HasKey(keys::kEventTypeKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        js_keyboard_event->GetString(keys::kEventTypeKey, &type));
  } else {
    error_ = keys::kInvalidKeyboardEventObjectError;
    return false;
  }

  if (type.compare(keys::kKeyboardEventTypeValueKeypress) == 0) {
    keyboard_event.type = WebInputEvent::Char;
  } else if (type.compare(keys::kKeyboardEventTypeValueKeydown) == 0) {
    keyboard_event.type = WebInputEvent::KeyDown;
  } else if (type.compare(keys::kKeyboardEventTypeValueKeyup) == 0) {
    keyboard_event.type = WebInputEvent::KeyUp;
  } else {
    error_ = keys::kInvalidKeyboardEventObjectError;
    return false;
  }

  int key_code;
  if (js_keyboard_event->HasKey(keys::kKeyboardEventKeyCodeKey)) {
    EXTENSION_FUNCTION_VALIDATE(js_keyboard_event->
        GetInteger(keys::kKeyboardEventKeyCodeKey, &key_code));
    keyboard_event.nativeKeyCode = key_code;
    keyboard_event.windowsKeyCode = key_code;
    keyboard_event.setKeyIdentifierFromWindowsKeyCode();
  }

  // Keypress = type character
  if (type.compare(keys::kKeyboardEventTypeValueKeypress) == 0) {
    int char_code;
    if (js_keyboard_event->HasKey(keys::kKeyboardEventCharCodeKey)) {
      EXTENSION_FUNCTION_VALIDATE(js_keyboard_event->
          GetInteger(keys::kKeyboardEventCharCodeKey, &char_code));
      keyboard_event.text[0] = char_code;
      keyboard_event.unmodifiedText[0] = char_code;
    } else {
      error_ = keys::kInvalidKeyboardEventObjectError;
      return false;
    }
  }

  EXTENSION_FUNCTION_VALIDATE(
      CopyModifiers(js_keyboard_event, &keyboard_event));

  // Forward the event.
  offscreen_tab->web_contents()->GetRenderViewHost()->
      ForwardKeyboardEvent(keyboard_event);

  return true;
}

SendMouseEventOffscreenTabFunction::SendMouseEventOffscreenTabFunction() {}
SendMouseEventOffscreenTabFunction::~SendMouseEventOffscreenTabFunction() {}

bool SendMouseEventOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(
          offscreen_tab_id, this, &offscreen_tab, &error_))
    return false;

  // JavaScript MouseEvent.
  DictionaryValue* js_mouse_event = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &js_mouse_event));

  std::string type;
  if (js_mouse_event->HasKey(keys::kEventTypeKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        js_mouse_event->GetString(keys::kEventTypeKey, &type));
  } else {
    error_ = keys::kInvalidMouseEventObjectError;
    return false;
  }

  if (type.compare(keys::kMouseEventTypeValueMousewheel) == 0) {
    WebKit::WebMouseWheelEvent wheel_event;

    wheel_event.type = WebInputEvent::MouseWheel;

    if (js_mouse_event->HasKey(keys::kMouseEventWheelDeltaXKey) &&
        js_mouse_event->HasKey(keys::kMouseEventWheelDeltaYKey)) {
      int delta_x, delta_y;
      EXTENSION_FUNCTION_VALIDATE(js_mouse_event->
          GetInteger(keys::kMouseEventWheelDeltaXKey, &delta_x));
      EXTENSION_FUNCTION_VALIDATE(js_mouse_event->
          GetInteger(keys::kMouseEventWheelDeltaYKey, &delta_y));
      wheel_event.deltaX = delta_x;
      wheel_event.deltaY = delta_y;
    } else {
      error_ = keys::kInvalidMouseEventObjectError;
      return false;
    }

    // Forward the event.
    offscreen_tab->web_contents()->GetRenderViewHost()->
        ForwardWheelEvent(wheel_event);
  } else {
    WebKit::WebMouseEvent mouse_event;

    if (type.compare(keys::kMouseEventTypeValueMousedown) == 0 ||
        type.compare(keys::kMouseEventTypeValueClick) == 0) {
      mouse_event.type = WebInputEvent::MouseDown;
    } else if (type.compare(keys::kMouseEventTypeValueMouseup) == 0) {
      mouse_event.type = WebInputEvent::MouseUp;
    } else if (type.compare(keys::kMouseEventTypeValueMousemove) == 0) {
      mouse_event.type = WebInputEvent::MouseMove;
    } else {
      error_ = keys::kInvalidMouseEventObjectError;
      return false;
    }

    int button;
    if (js_mouse_event->HasKey(keys::kMouseEventButtonKey)) {
      EXTENSION_FUNCTION_VALIDATE(
          js_mouse_event->GetInteger(keys::kMouseEventButtonKey, &button));
    } else {
      error_ = keys::kInvalidMouseEventObjectError;
      return false;
    }

    if (button == keys::kMouseEventButtonValueLeft) {
      mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
    } else if (button == keys::kMouseEventButtonValueMiddle) {
      mouse_event.button = WebKit::WebMouseEvent::ButtonMiddle;
    } else if (button == keys::kMouseEventButtonValueRight) {
      mouse_event.button = WebKit::WebMouseEvent::ButtonRight;
    } else {
      error_ = keys::kInvalidMouseEventObjectError;
      return false;
    }

    if (HasOptionalArgument(2)) {
      DictionaryValue* position = NULL;
      EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(2, &position));
      EXTENSION_FUNCTION_VALIDATE(position->GetInteger(
          keys::kMouseEventPositionXKey, &mouse_event.x));
      EXTENSION_FUNCTION_VALIDATE(position->GetInteger(
          keys::kMouseEventPositionYKey, &mouse_event.y));
    } else {
      error_ = keys::kNoMouseCoordinatesError;
      return false;
    }

    EXTENSION_FUNCTION_VALIDATE(CopyModifiers(js_mouse_event, &mouse_event));

    mouse_event.clickCount = 1;

    // Forward the event.
    offscreen_tab->web_contents()->GetRenderViewHost()->
        ForwardMouseEvent(mouse_event);

    // If the event is a click,
    // fire a mouseup event in addition to the mousedown.
    if (type.compare(keys::kMouseEventTypeValueClick) == 0) {
      mouse_event.type = WebInputEvent::MouseUp;
      offscreen_tab->web_contents()->GetRenderViewHost()->
          ForwardMouseEvent(mouse_event);
    }
  }

  return true;
}

ToDataUrlOffscreenTabFunction::ToDataUrlOffscreenTabFunction() {}
ToDataUrlOffscreenTabFunction::~ToDataUrlOffscreenTabFunction() {}

bool ToDataUrlOffscreenTabFunction::GetTabToCapture(
    WebContents** web_contents, TabContentsWrapper** wrapper) {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  // TODO(alexbost): We want to optimize this function in order to get more
  // image updates on the browser side. One improvement would be to implement
  // another hash map in order to get offscreen tabs in O(1).
  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(
          offscreen_tab_id, this, &offscreen_tab, &error_))
    return false;

  *web_contents = offscreen_tab->web_contents();
  *wrapper = offscreen_tab->tab_contents();
  return true;
}

UpdateOffscreenTabFunction::UpdateOffscreenTabFunction() {}
UpdateOffscreenTabFunction::~UpdateOffscreenTabFunction() {}

bool UpdateOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(
          offscreen_tab_id, this, &offscreen_tab, &error_))
    return false;

  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &update_props));

  tab_contents_ = offscreen_tab->tab_contents();
  bool is_async = false;
  if (!UpdateURLIfPresent(update_props, &is_async))
    return false;

  // Update the width and height, if specified.
  if (update_props->HasKey(tabs_keys::kWidthKey) ||
      update_props->HasKey(tabs_keys::kHeightKey)) {
    const gfx::Size& size =
        tab_contents_->web_contents()->GetView()->GetContainerSize();

    int width;
    if (update_props->HasKey(tabs_keys::kWidthKey))
      EXTENSION_FUNCTION_VALIDATE(
          update_props->GetInteger(tabs_keys::kWidthKey, &width));
    else
      width = size.width();

    int height;
    if (update_props->HasKey(tabs_keys::kHeightKey))
      EXTENSION_FUNCTION_VALIDATE(
          update_props->GetInteger(tabs_keys::kHeightKey, &height));
    else
      height = size.height();

    offscreen_tab->SetSize(width, height);
  }

  // The response is sent from UpdateTabFunction::OnExecuteCodeFinish in the
  // async case (when a "javascript": URL is sent to a tab).
  if (!is_async)
    SendResponse(true);

  return true;
}

void UpdateOffscreenTabFunction::PopulateResult() {
  // There's no result associated with this callback.
}
