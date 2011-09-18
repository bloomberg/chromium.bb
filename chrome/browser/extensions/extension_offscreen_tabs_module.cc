// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_offscreen_tabs_module.h"

#include <algorithm>
#include <vector>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_offscreen_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/content_client.h"
#include "content/common/notification_service.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

using WebKit::WebInputEvent;

namespace keys = extension_offscreen_tabs_module_constants;

namespace {

class ParentTab;

// Offscreen Tab ---------------------------------------------------------------

// This class is responsible for the creation and destruction of offscreen tabs,
// as well as dispatching an onUpdated event.
class OffscreenTab : public NotificationObserver {
 public:
  OffscreenTab();
  virtual ~OffscreenTab();
  void Init(const GURL& url,
            const int width,
            const int height,
            Profile* profile,
            ParentTab* parent_tab);

  TabContentsWrapper* tab() { return tab_.get(); }
  TabContents* contents() { return tab_.get()->tab_contents(); }
  ParentTab* parent_tab() { return parent_tab_; }
  DictionaryValue* CreateValue();  // Creates an offscreen tab object returned
                                   // by the API methods.
                                   // The caller owns the returned value.

  void SetURL(const GURL& url);
  void SetSize(int width, int height);

 private:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  scoped_ptr<TabContentsWrapper> tab_;  // TabContentsWrapper associated with
                                        // this offscreen tab.
  ParentTab* parent_tab_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenTab);
};

typedef std::vector<OffscreenTab*> TabVector;
typedef TabVector::iterator TabIterator;
typedef TabVector::const_iterator ConstTabIterator;

// ParentTab -------------------------------------------------------------------

// Holds info about a tab that has spawned at least one offscreen tab.
// Each ParentTab keeps track of its child offscreen tabs. The ParentTab is also
// responsible for killing its children when it navigates away or gets closed.
class ParentTab : public NotificationObserver {
 public:
  ParentTab();
  virtual ~ParentTab();
  void Init(TabContents* tab_contents, const std::string& extension_id);

  TabContentsWrapper* tab() { return tab_; }
  TabContents* contents() { return tab_->tab_contents(); }
  const TabVector& offscreen_tabs() { return offscreen_tabs_; }
  const std::string& extension_id() const { return extension_id_; }

  // Tab takes ownership of OffscreenTab.
  void AddOffscreenTab(OffscreenTab *tab);
  // This deletes the OffscreenTab.
  void RemoveOffscreenTab(OffscreenTab *tab);

 private:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  TabContentsWrapper* tab_;  // TabContentsWrapper associated with this tab.
  TabVector offscreen_tabs_;  // Offscreen tabs spawned by this tab.
  std::string extension_id_;  // Id of the extension running in this tab.

  DISALLOW_COPY_AND_ASSIGN(ParentTab);
};

// Map -------------------------------------------------------------------------

// This map keeps track of all tabs that are happy parents of offscreen tabs.
class Map {
 public:
  Map();
  ~Map();

  // Gets an offscreen tab by ID.
  bool GetOffscreenTab(const int offscreen_tab_id,
                       ExtensionFunctionDispatcher* dispatcher,
                       Profile* profile,
                       OffscreenTab** offscreen_tab,
                       std::string* error_message);
  // Gets a parent tab by ID.
  bool GetParentTab(const int parent_tab_id,
                    ParentTab** tab,
                    std::string* error_message);
  // Creates a mapping between a parent tab and an offscreen tab.
  bool AddOffscreenTab(OffscreenTab* offscreen_tab,
                       const GURL& url,
                       const int width,
                       const int height,
                       Profile* profile,
                       ExtensionFunctionDispatcher* dispatcher,
                       const std::string& ext_id,
                       std::string* error_message);
  // Removes the mapping between a parent tab and an offscreen tab.
  // May cause the Tab object associated with the parent to be deleted.
  bool RemoveOffscreenTab(const int offscreen_tab_id,
                          ExtensionFunctionDispatcher* dispatcher,
                          Profile* profile,
                          std::string* error_message);
  // Removes a parent tab from the map along with its child offscreen tabs.
  // It is called by the destructor of a ParentTab.
  bool RemoveParentTab(const int parent_tab_id, std::string* error_message);

 private:
  typedef base::hash_map<int, ParentTab*> TabMap;
  TabMap map;

  DISALLOW_COPY_AND_ASSIGN(Map);
};

// Variables -------------------------------------------------------------------

Map* map = NULL;

// We are assuming that offscreen tabs will not be created by background pages
// with the exception of the API test background page. We keep track of the
// offscreen tabs associated with the test API background page via this variable
// These tab contents are created just for convenience and do not do anything.
// TODO(alexbost): Think about handling multiple background pages each spawning
// offscreen tabs. Would background pages want to spawn offscreen tabs?
// If not, we need to somehow distinguish between the test background page and
// regular background pages and disallow offscreen tab creation for the latter.
TabContents* background_page_tab_contents = NULL;

// Util ------------------------------------------------------------------------

// Gets the map of parent tabs to offscreen tabs.
Map* GetMap() {
  if (map == NULL)
    map = new Map();

  return map;
}

// Gets the TabContents associated with the test API background page.
TabContents* GetBackgroundPageTabContents(Profile* profile) {
  if (background_page_tab_contents == NULL) {
    background_page_tab_contents =
        new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL, NULL);
    new TabContentsWrapper(background_page_tab_contents);
  }

  return background_page_tab_contents;
}

// Gets the contents of the tab that instantiated the extension API call.
// In the case of background pages we use tab contents created by us.
bool GetCurrentTabContents(ExtensionFunctionDispatcher* dispatcher,
                           Profile* profile,
                           TabContents** tab_contents,
                           std::string* error_message) {
  *tab_contents = dispatcher->delegate()->GetAssociatedTabContents();

  // Background page (no associated tab contents).
  if (!*tab_contents)
    *tab_contents = GetBackgroundPageTabContents(profile);

  if (*tab_contents)
    return true;

  *error_message = keys::kCurrentTabNotFound;
  return false;
}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
// Takes |url_string| and returns a GURL which is either valid and absolute
// or invalid. If |url_string| is not directly interpretable as a valid (it is
// likely a relative URL) an attempt is made to resolve it. |extension| is
// provided so it can be resolved relative to its extension base
// (chrome-extension://<id>/). Using the source frame url would be more correct,
// but because the api shipped with urls resolved relative to their extension
// base, we decided it wasn't worth breaking existing extensions to fix.
GURL ResolvePossiblyRelativeURL(const std::string& url_string,
                                const Extension* extension) {
  GURL url = GURL(url_string);
  if (!url.is_valid())
    url = extension->GetResourceURL(url_string);

  return url;
}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
bool IsCrashURL(const GURL& url) {
  // Check a fixed-up URL, to normalize the scheme and parse hosts correctly.
  GURL fixed_url =
      URLFixerUpper::FixupURL(url.possibly_invalid_spec(), std::string());
  return (fixed_url.SchemeIs(chrome::kChromeUIScheme) &&
          (fixed_url.host() == chrome::kChromeUIBrowserCrashHost ||
           fixed_url.host() == chrome::kChromeUICrashHost));
}

// Offscreen Tab ---------------------------------------------------------------

OffscreenTab::OffscreenTab()
    : parent_tab_(NULL) {
}

OffscreenTab::~OffscreenTab() {}

void OffscreenTab::Init(const GURL& url,
                        const int width,
                        const int height,
                        Profile* profile,
                        ParentTab* parent_tab) {
  // Create the offscreen tab.
  TabContents* tab_contents = new TabContents(
      profile, NULL, MSG_ROUTING_NONE, NULL, NULL);
  tab_.reset(new TabContentsWrapper(tab_contents));

  SetSize(width, height);  // Setting the size starts the renderer.
  SetURL(url);
  parent_tab_ = parent_tab;

  // Register for tab notifications.
  registrar_.Add(this,
                 content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&contents()->controller()));
}

DictionaryValue* OffscreenTab::CreateValue() {
  DictionaryValue* result = new DictionaryValue();

  result->SetInteger(keys::kIdKey, ExtensionTabUtil::GetTabId(contents()));
  result->SetString(keys::kUrlKey, contents()->GetURL().spec());
  result->SetInteger(keys::kWidthKey,
      contents()->view()->GetContainerSize().width());
  result->SetInteger(keys::kHeightKey,
      contents()->view()->GetContainerSize().height());

  return result;
}

void OffscreenTab::SetURL(const GURL& url) {
  contents()->controller().LoadURL(
      url, GURL(), PageTransition::LINK, std::string());
}

void OffscreenTab::SetSize(int width, int height) {
  contents()->view()->SizeContents(gfx::Size(width, height));
}

void OffscreenTab::Observe(int type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);

  DictionaryValue* changed_properties = new DictionaryValue();
  changed_properties->SetString(keys::kUrlKey, contents()->GetURL().spec());

  ListValue args;
  args.Append(
      Value::CreateIntegerValue(ExtensionTabUtil::GetTabId(contents())));
  args.Append(changed_properties);
  args.Append(CreateValue());
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  ListValue event_args;
  event_args.Set(0, Value::CreateStringValue(keys::kEventOnUpdated));
  event_args.Set(1, Value::CreateStringValue(json_args));

  // Dispatch an onUpdated event.
  // The primary use case for broadcasting the event is
  // when the offscreen tab is generated by a test API background page.
  if (parent_tab_->contents() == background_page_tab_contents) {
    Profile* profile = parent_tab_->tab()->profile();
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        keys::kEventOnUpdated, json_args, profile, GURL());
  } else {
    // Send a routed event directly to the parent tab.
    if (parent_tab_->contents()->render_view_host() &&
        parent_tab_->contents()->render_view_host()->process())
      parent_tab_->contents()->render_view_host()->process()->Send(
          new ExtensionMsg_MessageInvoke(
              parent_tab_->contents()->render_view_host()->routing_id(),
              parent_tab_->extension_id(),
              keys::kDispatchEvent,
              event_args,
              GURL()));
  }
}

// ParentTab -------------------------------------------------------------------

ParentTab::ParentTab()
    : tab_(NULL) {
}

ParentTab::~ParentTab() {
  // Kill child offscreen tabs.
  STLDeleteElements(&offscreen_tabs_);

  bool removed = GetMap()->RemoveParentTab(
      ExtensionTabUtil::GetTabId(contents()), new std::string());
  DCHECK(removed);
}

void ParentTab::Init(TabContents* tab_contents,
                     const std::string& extension_id) {
  DCHECK(tab_contents);

  tab_ = TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  DCHECK(tab_);

  extension_id_ = extension_id;

  // Register for tab notifications.
  registrar_.Add(this,
                 content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&contents()->controller()));

  registrar_.Add(this,
                 content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(contents()));
}

void ParentTab::AddOffscreenTab(OffscreenTab *offscreen_tab) {
  offscreen_tabs_.push_back(offscreen_tab);
}

void ParentTab::RemoveOffscreenTab(OffscreenTab *offscreen_tab) {
  TabIterator it_tab = std::find(
      offscreen_tabs_.begin(), offscreen_tabs_.end(), offscreen_tab);
  offscreen_tabs_.erase(it_tab);

  delete offscreen_tab;
}

void ParentTab::Observe(int type,
                        const NotificationSource& source,
                        const NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_NAV_ENTRY_COMMITTED ||
         type == content::NOTIFICATION_TAB_CONTENTS_DESTROYED);

  delete this;
}

// Map -------------------------------------------------------------------------

Map::Map() {}
Map::~Map() {}

bool Map::GetOffscreenTab(const int offscreen_tab_id,
                          ExtensionFunctionDispatcher* dispatcher,
                          Profile* profile,
                          OffscreenTab** offscreen_tab,
                          std::string* error_message) {
  // Ensure that the current tab is the parent of the offscreen tab.
  TabContents* tab_contents = NULL;
  if (!GetCurrentTabContents(dispatcher, profile, &tab_contents, error_message))
    return false;

  ParentTab* parent_tab = NULL;
  if (!GetParentTab(
      ExtensionTabUtil::GetTabId(tab_contents), &parent_tab, error_message))
    return false;

  const TabVector& offscreen_tabs = parent_tab->offscreen_tabs();

  for (ConstTabIterator it = offscreen_tabs.begin();
      it != offscreen_tabs.end(); ++it) {
    if (ExtensionTabUtil::GetTabId((*it)->contents()) == offscreen_tab_id) {
      *offscreen_tab = *it;
      return true;
    }
  }

  *error_message = ExtensionErrorUtils::FormatErrorMessage(
      keys::kOffscreenTabNotFoundError, base::IntToString(offscreen_tab_id));
  return false;
}

bool Map::GetParentTab(const int parent_tab_id,
                       ParentTab** parent_tab,
                       std::string* error_message) {
  TabMap::iterator it = map.find(parent_tab_id);

  if (it == map.end()) {
    *error_message = ExtensionErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::IntToString(parent_tab_id));
    return false;
  }

  *parent_tab = it->second;

  return true;
}

bool Map::AddOffscreenTab(OffscreenTab* offscreen_tab,
                          const GURL& url,
                          const int width,
                          const int height,
                          Profile* profile,
                          ExtensionFunctionDispatcher* dispatcher,
                          const std::string& ext_id,
                          std::string* error_message) {
  // Get parent tab.
  TabContents* tab_contents = NULL;
  if (!GetCurrentTabContents(dispatcher, profile, &tab_contents, error_message))
    return false;
  int offscreen_tab_id = ExtensionTabUtil::GetTabId(tab_contents);

  ParentTab* parent_tab = NULL;
  if (!GetParentTab(offscreen_tab_id, &parent_tab, error_message)) {
    parent_tab = new ParentTab();
    parent_tab->Init(tab_contents, ext_id);

    map[offscreen_tab_id] = parent_tab;
  }

  offscreen_tab->Init(url, width, height, profile, parent_tab);

  // Add child to parent.
  parent_tab->AddOffscreenTab(offscreen_tab);

  return true;
}

bool Map::RemoveOffscreenTab(const int offscreen_tab_id,
                             ExtensionFunctionDispatcher* dispatcher,
                             Profile* profile,
                             std::string* error_message) {
  OffscreenTab* offscreen_tab = NULL;
  if (!GetOffscreenTab(offscreen_tab_id, dispatcher, profile,
                       &offscreen_tab, error_message))
    return false;

  ParentTab* parent_tab = offscreen_tab->parent_tab();

  parent_tab->RemoveOffscreenTab(offscreen_tab);
  offscreen_tab = NULL;

  // If this was the last offscreen tab for the parent tab, remove the parent.
  if (parent_tab->offscreen_tabs().empty())
    delete parent_tab;  // Causes tab to be removed from the map!

  return true;
}

bool Map::RemoveParentTab(const int parent_tab_id, std::string* error_message) {
  if (map.find(parent_tab_id) == map.end()) {
    *error_message = ExtensionErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::IntToString(parent_tab_id));
    return false;
  }

  map.erase(parent_tab_id);

  return true;
}

}  // namespace

// API functions ---------------------------------------------------------------

// create ----------------------------------------------------------------------

CreateOffscreenTabFunction::CreateOffscreenTabFunction() {}
CreateOffscreenTabFunction::~CreateOffscreenTabFunction() {}

bool CreateOffscreenTabFunction::RunImpl() {
  DictionaryValue* create_props;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &create_props));

  std::string url_string;
  GURL url;
  EXTENSION_FUNCTION_VALIDATE(
      create_props->GetString(keys::kUrlKey, &url_string));
  url = ResolvePossiblyRelativeURL(url_string, GetExtension());
  if (!url.is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kInvalidUrlError, url_string);
    return false;
  }
  if (IsCrashURL(url)) {
    error_ = keys::kNoCrashBrowserError;
    return false;
  }

  gfx::Rect window_bounds;
  bool maximized;
  if (!create_props->HasKey(keys::kWidthKey) ||
      !create_props->HasKey(keys::kHeightKey)) {
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      error_ = keys::kNoCurrentWindowError;
      return false;
    }

    WindowSizer::GetBrowserWindowBounds(std::string(), gfx::Rect(),
                                        browser, &window_bounds,
                                        &maximized);
  }

  int width = window_bounds.width();
  if (create_props->HasKey(keys::kWidthKey))
    EXTENSION_FUNCTION_VALIDATE(
        create_props->GetInteger(keys::kWidthKey, &width));

  int height = window_bounds.height();
  if (create_props->HasKey(keys::kHeightKey))
    EXTENSION_FUNCTION_VALIDATE(
        create_props->GetInteger(keys::kHeightKey, &height));

  OffscreenTab* offscreen_tab = new OffscreenTab();

  // Add the offscreen tab to the map so we don't lose track of it.
  if (!GetMap()->AddOffscreenTab(offscreen_tab, url, width, height, profile_,
                                 dispatcher(), extension_id(), &error_)) {
    delete offscreen_tab;  // Prevent leaking of offscreen tab.
    return false;
  }

  // TODO(alexbost): Maybe the callback is called too soon. It should probably
  // be called once we have navigated to the url.
  if (has_callback()) {
    result_.reset(offscreen_tab->CreateValue());
    SendResponse(true);
  }

  return true;
}

// get -------------------------------------------------------------------------

GetOffscreenTabFunction::GetOffscreenTabFunction() {}
GetOffscreenTabFunction::~GetOffscreenTabFunction() {}

bool GetOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->
      GetOffscreenTab(offscreen_tab_id, dispatcher(), profile_,
                      &offscreen_tab, &error_)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kOffscreenTabNotFoundError, base::IntToString(offscreen_tab_id));
    return false;
  }

  if (has_callback()) {
    result_.reset(offscreen_tab->CreateValue());
    SendResponse(true);
  }

  return true;
}

// getAll ----------------------------------------------------------------------

GetAllOffscreenTabFunction::GetAllOffscreenTabFunction() {}
GetAllOffscreenTabFunction::~GetAllOffscreenTabFunction() {}

bool GetAllOffscreenTabFunction::RunImpl() {
  TabContents* tab_contents = NULL;
  if (!GetCurrentTabContents(dispatcher(), profile_, &tab_contents, &error_))
    return false;

  ParentTab* parent_tab = NULL;
  if (!GetMap()->
      GetParentTab(ExtensionTabUtil::GetTabId(tab_contents),
                                              &parent_tab, &error_))
    return false;

  const TabVector& offscreen_tabs = parent_tab->offscreen_tabs();

  ListValue* tab_list = new ListValue();
  for (ConstTabIterator it_tab = offscreen_tabs.begin();
      it_tab != offscreen_tabs.end(); ++it_tab)
    tab_list->Append((*it_tab)->CreateValue());

  if (has_callback()) {
    result_.reset(tab_list);
    SendResponse(true);
  }

  return true;
}

// remove ----------------------------------------------------------------------

RemoveOffscreenTabFunction::RemoveOffscreenTabFunction() {}
RemoveOffscreenTabFunction::~RemoveOffscreenTabFunction() {}

bool RemoveOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(offscreen_tab_id, dispatcher(), profile_,
                                 &offscreen_tab, &error_))
    return false;

  if (!GetMap()->RemoveOffscreenTab(offscreen_tab_id, dispatcher(), profile_,
                                    &error_))
    return false;

  return true;
}

// sendKeyboardEvent -----------------------------------------------------------

SendKeyboardEventOffscreenTabFunction::
    SendKeyboardEventOffscreenTabFunction() {}
SendKeyboardEventOffscreenTabFunction::
    ~SendKeyboardEventOffscreenTabFunction() {}

bool SendKeyboardEventOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(offscreen_tab_id, dispatcher(), profile_,
                                 &offscreen_tab, &error_))
    return false;

  // JavaScript KeyboardEvent.
  DictionaryValue* js_keyboard_event = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &js_keyboard_event));

  NativeWebKeyboardEvent keyboard_event;

  std::string type;
  if (js_keyboard_event->HasKey(keys::kKeyboardEventTypeKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        js_keyboard_event->GetString(keys::kKeyboardEventTypeKey, &type));
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
  } else {
    error_ = keys::kInvalidKeyboardEventObjectError;
    return false;
  }

  keyboard_event.nativeKeyCode = key_code;
  keyboard_event.windowsKeyCode = key_code;
  keyboard_event.setKeyIdentifierFromWindowsKeyCode();

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

  bool alt_key;
  if (js_keyboard_event->HasKey(keys::kKeyboardEventAltKeyKey))
    EXTENSION_FUNCTION_VALIDATE(js_keyboard_event->
        GetBoolean(keys::kKeyboardEventAltKeyKey, &alt_key));
  if (alt_key)
    keyboard_event.modifiers |= WebInputEvent::AltKey;

  bool ctrl_key;
  if (js_keyboard_event->HasKey(keys::kKeyboardEventCtrlKeyKey))
    EXTENSION_FUNCTION_VALIDATE(js_keyboard_event->
        GetBoolean(keys::kKeyboardEventCtrlKeyKey, &ctrl_key));
  if (ctrl_key)
    keyboard_event.modifiers |= WebInputEvent::ControlKey;

  bool meta_key = false;
  if (js_keyboard_event->HasKey(keys::kMouseEventMetaKeyKey))
    EXTENSION_FUNCTION_VALIDATE(js_keyboard_event->
        GetBoolean(keys::kMouseEventMetaKeyKey, &meta_key));
  if (meta_key)
    keyboard_event.modifiers |= WebInputEvent::MetaKey;

  bool shift_key;
  if (js_keyboard_event->HasKey(keys::kKeyboardEventShiftKeyKey))
    EXTENSION_FUNCTION_VALIDATE(js_keyboard_event->
        GetBoolean(keys::kKeyboardEventShiftKeyKey, &shift_key));
  if (shift_key)
    keyboard_event.modifiers |= WebInputEvent::ShiftKey;

  // Forward the event.
  offscreen_tab->contents()->render_view_host()->
      ForwardKeyboardEvent(keyboard_event);

  if (has_callback()) {
    result_.reset(offscreen_tab->CreateValue());
    SendResponse(true);
  }

  return true;
}

// sendMouseEvent --------------------------------------------------------------

SendMouseEventOffscreenTabFunction::SendMouseEventOffscreenTabFunction() {}
SendMouseEventOffscreenTabFunction::~SendMouseEventOffscreenTabFunction() {}

bool SendMouseEventOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(offscreen_tab_id, dispatcher(), profile_,
                                 &offscreen_tab, &error_))
    return false;

  // JavaScript MouseEvent.
  DictionaryValue* js_mouse_event = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &js_mouse_event));

  std::string type;
  if (js_mouse_event->HasKey(keys::kMouseEventTypeKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        js_mouse_event->GetString(keys::kMouseEventTypeKey, &type));
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
    offscreen_tab->contents()->render_view_host()->
        ForwardWheelEvent(wheel_event);
  } else {
    WebKit::WebMouseEvent mouse_event;

    if (type.compare(keys::kMouseEventTypeValueMousedown) == 0 ||
        type.compare(keys::kMouseEventTypeValueClick) == 0) {
      mouse_event.type = WebKit::WebInputEvent::MouseDown;
    } else if (type.compare(keys::kMouseEventTypeValueMouseup) == 0) {
      mouse_event.type = WebKit::WebInputEvent::MouseUp;
    } else if (type.compare(keys::kMouseEventTypeValueMousemove) == 0) {
      mouse_event.type = WebKit::WebInputEvent::MouseMove;
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

    if (HasOptionalArgument(2) && HasOptionalArgument(3)) {
      EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &mouse_event.x));
      EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(3, &mouse_event.y));
    } else {
      error_ = keys::kNoMouseCoordinatesError;
      return false;
    }

    bool alt_key = false;
    if (js_mouse_event->HasKey(keys::kMouseEventAltKeyKey))
      EXTENSION_FUNCTION_VALIDATE(js_mouse_event->
          GetBoolean(keys::kMouseEventAltKeyKey, &alt_key));
    if (alt_key)
      mouse_event.modifiers |= WebInputEvent::AltKey;

    bool ctrl_key = false;
    if (js_mouse_event->HasKey(keys::kMouseEventCtrlKeyKey))
      EXTENSION_FUNCTION_VALIDATE(js_mouse_event->
          GetBoolean(keys::kMouseEventCtrlKeyKey, &ctrl_key));
    if (ctrl_key)
      mouse_event.modifiers |= WebInputEvent::ControlKey;

    bool meta_key = false;
    if (js_mouse_event->HasKey(keys::kMouseEventMetaKeyKey))
      EXTENSION_FUNCTION_VALIDATE(js_mouse_event->
          GetBoolean(keys::kMouseEventMetaKeyKey, &meta_key));
    if (meta_key)
      mouse_event.modifiers |= WebInputEvent::MetaKey;

    bool shift_key = false;
    if (js_mouse_event->HasKey(keys::kMouseEventShiftKeyKey))
      EXTENSION_FUNCTION_VALIDATE(js_mouse_event->
          GetBoolean(keys::kMouseEventShiftKeyKey, &shift_key));
    if (shift_key)
      mouse_event.modifiers |= WebInputEvent::ShiftKey;

    mouse_event.clickCount = 1;

    // Forward the event.
    offscreen_tab->contents()->render_view_host()->
        ForwardMouseEvent(mouse_event);

    // If the event is a click,
    // fire a mouseup event in addition to the mousedown.
    if (type.compare(keys::kMouseEventTypeValueClick) == 0) {
      mouse_event.type = WebKit::WebInputEvent::MouseUp;
      offscreen_tab->contents()->render_view_host()->
          ForwardMouseEvent(mouse_event);
    }
  }

  if (has_callback()) {
    result_.reset(offscreen_tab->CreateValue());
    SendResponse(true);
  }

  return true;
}

// toDataUrl -------------------------------------------------------------------

ToDataUrlOffscreenTabFunction::ToDataUrlOffscreenTabFunction() {}
ToDataUrlOffscreenTabFunction::~ToDataUrlOffscreenTabFunction() {}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
// TODO(alexbost): We want to optimize this function in order to get more image
// updates on the browser side. One improvement would be to implement another
// hash map in order to get offscreen tabs in O(1).
bool ToDataUrlOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(offscreen_tab_id, dispatcher(), profile_,
                                 &offscreen_tab, &error_))
    return false;

  image_format_ = FORMAT_JPEG;  // Default format is JPEG.
  image_quality_ = kDefaultQuality;  // Default quality setting.

  if (HasOptionalArgument(1)) {
    DictionaryValue* options;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &options));

    if (options->HasKey(keys::kFormatKey)) {
      std::string format;
      EXTENSION_FUNCTION_VALIDATE(
          options->GetString(keys::kFormatKey, &format));

      if (format == keys::kFormatValueJpeg) {
        image_format_ = FORMAT_JPEG;
      } else if (format == keys::kFormatValuePng) {
        image_format_ = FORMAT_PNG;
      } else {
        // Schema validation should make this unreachable.
        EXTENSION_FUNCTION_VALIDATE(0);
      }
    }

    if (options->HasKey(keys::kQualityKey)) {
      EXTENSION_FUNCTION_VALIDATE(
          options->GetInteger(keys::kQualityKey, &image_quality_));
    }
  }

  // captureVisibleTab() can return an image containing sensitive information
  // that the browser would otherwise protect.  Ensure the extension has
  // permission to do this.
  if (!GetExtension()->
      CanCaptureVisiblePage(offscreen_tab->contents()->GetURL(), &error_))
    return false;

// The backing store approach works on Linux but not on Mac.
// TODO(alexbost): Test on Windows
#if !defined(OS_MACOSX)
  RenderViewHost* render_view_host =
      offscreen_tab->contents()->render_view_host();

  // If a backing store is cached for the tab we want to capture,
  // and it can be copied into a bitmap, then use it to generate the image.
  BackingStore* backing_store = render_view_host->GetBackingStore(false);
  if (backing_store && CaptureSnapshotFromBackingStore(backing_store))
    return true;
#endif

  // Ask the renderer for a snapshot of the tab.
  TabContentsWrapper* tab_wrapper = offscreen_tab->tab();
  tab_wrapper->CaptureSnapshot();
  registrar_.Add(this,
               chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN,
               Source<TabContentsWrapper>(tab_wrapper));

  AddRef();  // Balanced in ToDataUrlOffscreenTabFunction::Observe().

  return true;
}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
// Build the image of a tab's contents out of a backing store.
// This may fail if we can not copy a backing store into a bitmap.
// For example, some uncommon X11 visual modes are not supported by
// CopyFromBackingStore().
bool ToDataUrlOffscreenTabFunction::CaptureSnapshotFromBackingStore(
    BackingStore* backing_store) {

  skia::PlatformCanvas temp_canvas;
  if (!backing_store->CopyFromBackingStore(gfx::Rect(backing_store->size()),
                                           &temp_canvas)) {
    return false;
  }
  VLOG(1) << "captureVisibleTab() got image from backing store.";

  SendResultFromBitmap(
      skia::GetTopDevice(temp_canvas)->accessBitmap(false));
  return true;
}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
void ToDataUrlOffscreenTabFunction::Observe(int type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN);

  const SkBitmap *screen_capture = Details<const SkBitmap>(details).ptr();
  const bool error = screen_capture->empty();

  if (error) {
    error_ = keys::kInternalVisibleTabCaptureError;
    SendResponse(false);
  } else {
    VLOG(1) << "Got image from renderer.";
    SendResultFromBitmap(*screen_capture);
  }

  Release();  // Balanced in ToDataUrlOffscreenTabFunction::RunImpl().
}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void ToDataUrlOffscreenTabFunction::SendResultFromBitmap(
    const SkBitmap& screen_capture) {
  std::vector<unsigned char> data;
  SkAutoLockPixels screen_capture_lock(screen_capture);
  bool encoded = false;
  std::string mime_type;
  switch (image_format_) {
    case FORMAT_JPEG:
      encoded = gfx::JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(screen_capture.getAddr32(0, 0)),
          gfx::JPEGCodec::FORMAT_SkBitmap,
          screen_capture.width(),
          screen_capture.height(),
          static_cast<int>(screen_capture.rowBytes()),
          image_quality_,
          &data);
      mime_type = keys::kMimeTypeJpeg;
      break;
    case FORMAT_PNG:
      encoded = gfx::PNGCodec::EncodeBGRASkBitmap(
          screen_capture,
          true,  // Discard transparency.
          &data);
      mime_type = keys::kMimeTypePng;
      break;
    default:
      NOTREACHED() << "Invalid image format.";
  }

  if (!encoded) {
    error_ = keys::kInternalVisibleTabCaptureError;
    SendResponse(false);
    return;
  }

  std::string base64_result;
  base::StringPiece stream_as_string(
      reinterpret_cast<const char*>(vector_as_array(&data)), data.size());

  base::Base64Encode(stream_as_string, &base64_result);
  base64_result.insert(0, base::StringPrintf("data:%s;base64,",
                                             mime_type.c_str()));
  result_.reset(new StringValue(base64_result));
  SendResponse(true);
}

// update ----------------------------------------------------------------------

UpdateOffscreenTabFunction::UpdateOffscreenTabFunction() {}
UpdateOffscreenTabFunction::~UpdateOffscreenTabFunction() {}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
bool UpdateOffscreenTabFunction::RunImpl() {
  int offscreen_tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &offscreen_tab_id));

  OffscreenTab* offscreen_tab = NULL;
  if (!GetMap()->GetOffscreenTab(offscreen_tab_id, dispatcher(), profile_,
                                 &offscreen_tab, &error_))
    return false;

  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &update_props));

  // Url
  if (update_props->HasKey(keys::kUrlKey)) {
    std::string url_string;
    GURL url;
    EXTENSION_FUNCTION_VALIDATE(
        update_props->GetString(keys::kUrlKey, &url_string));
    url = ResolvePossiblyRelativeURL(url_string, GetExtension());
    if (!url.is_valid()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          keys::kInvalidUrlError, url_string);
      return false;
    }
    if (IsCrashURL(url)) {
      error_ = keys::kNoCrashBrowserError;
      return false;
    }

    // JavaScript URLs can do the same kinds of things as cross-origin XHR, so
    // we need to check host permissions before allowing them.
    if (url.SchemeIs(chrome::kJavaScriptScheme)) {
      if (!GetExtension()->CanExecuteScriptOnPage(
          offscreen_tab->contents()->GetURL(), NULL, &error_)) {
        return false;
      }

      ExtensionMsg_ExecuteCode_Params params;
      params.request_id = request_id();
      params.extension_id = extension_id();
      params.is_javascript = true;
      params.code = url.path();
      params.all_frames = false;
      params.in_main_world = true;

      RenderViewHost* render_view_host =
          offscreen_tab->contents()->render_view_host();
      render_view_host->Send(
          new ExtensionMsg_ExecuteCode(render_view_host->routing_id(),
                                       params));

      Observe(offscreen_tab->contents());
      AddRef();  // balanced in Observe()

      return true;
    }

    offscreen_tab->SetURL(url);

    // The URL of a tab contents never actually changes to a JavaScript URL, so
    // this check only makes sense in other cases.
    if (!url.SchemeIs(chrome::kJavaScriptScheme))
      DCHECK_EQ(url.spec(), offscreen_tab->contents()->GetURL().spec());
  }

  // Width and height
  if (update_props->HasKey(keys::kWidthKey) ||
      update_props->HasKey(keys::kHeightKey)) {
    int width;
    if (update_props->HasKey(keys::kWidthKey))
      EXTENSION_FUNCTION_VALIDATE(
          update_props->GetInteger(keys::kWidthKey, &width));
    else
      offscreen_tab->contents()->view()->GetContainerSize().width();

    int height;
    if (update_props->HasKey(keys::kHeightKey))
      EXTENSION_FUNCTION_VALIDATE(
          update_props->GetInteger(keys::kHeightKey, &height));
    else
      offscreen_tab->contents()->view()->GetContainerSize().height();

    offscreen_tab->SetSize(width, height);
  }

  // Callback
  if (has_callback()) {
    result_.reset(offscreen_tab->CreateValue());
    SendResponse(true);
  }

  return true;
}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
bool UpdateOffscreenTabFunction::OnMessageReceived(
    const IPC::Message& message) {
  if (message.type() != ExtensionHostMsg_ExecuteCodeFinished::ID)
    return false;

  int message_request_id;
  void* iter = NULL;
  if (!message.ReadInt(&iter, &message_request_id)) {
    NOTREACHED() << "malformed extension message";
    return true;
  }

  if (message_request_id != request_id())
    return false;

  IPC_BEGIN_MESSAGE_MAP(UpdateOffscreenTabFunction, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ExecuteCodeFinished,
                        OnExecuteCodeFinished)
  IPC_END_MESSAGE_MAP()
  return true;
}

// TODO(alexbost): Needs refactoring. Similar method in extension_tabs_module.
void UpdateOffscreenTabFunction::
    OnExecuteCodeFinished(int request_id,
                          bool success,
                          const std::string& error) {
  if (!error.empty()) {
    DCHECK(!success);
    error_ = error;
  }

  SendResponse(success);

  Observe(NULL);
  Release();  // balanced in Execute()
}

