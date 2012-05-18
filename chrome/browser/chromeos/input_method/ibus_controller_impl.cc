// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller_impl.h"

#include <algorithm>  // for std::reverse.
#include <cstdio>
#include <cstring>  // for std::strcmp.
#include <set>
#include <sstream>
#include <stack>
#include <utility>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/string_split.h"
#include "chrome/browser/chromeos/input_method/input_method_config.h"
#include "chrome/browser/chromeos/input_method/input_method_property.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"

// TODO(nona): Remove libibus dependency from this file. Then, write unit tests
// for all functions in this file. crbug.com/26334
#if defined(HAVE_IBUS)
#include <ibus.h>
#endif

namespace {

// Finds a property which has |new_prop.key| from |prop_list|, and replaces the
// property with |new_prop|. Returns true if such a property is found.
bool FindAndUpdateProperty(
    const chromeos::input_method::InputMethodProperty& new_prop,
    chromeos::input_method::InputMethodPropertyList* prop_list) {
  for (size_t i = 0; i < prop_list->size(); ++i) {
    chromeos::input_method::InputMethodProperty& prop = prop_list->at(i);
    if (prop.key == new_prop.key) {
      const int saved_id = prop.selection_item_id;
      // Update the list except the radio id. As written in
      // chromeos_input_method.h, |prop.selection_item_id| is dummy.
      prop = new_prop;
      prop.selection_item_id = saved_id;
      return true;
    }
  }
  return false;
}

}  // namespace

namespace chromeos {
namespace input_method {

#if defined(HAVE_IBUS)
const char kPanelObjectKey[] = "panel-object";

namespace {

const char* Or(const char* str1, const char* str2) {
  return str1 ? str1 : str2;
}

// Returns true if |key| is blacklisted.
bool PropertyKeyIsBlacklisted(const char* key) {
  // The list of input method property keys that we don't handle.
  static const char* kInputMethodPropertyKeysBlacklist[] = {
    "setup",  // used in ibus-m17n.
    "status",  // used in ibus-m17n.
  };
  for (size_t i = 0; i < arraysize(kInputMethodPropertyKeysBlacklist); ++i) {
    if (!std::strcmp(key, kInputMethodPropertyKeysBlacklist[i]))
      return true;
  }
  return false;
}

// Returns IBusInputContext for |input_context_path|. NULL on errors.
IBusInputContext* GetInputContext(const std::string& input_context_path,
                                  IBusBus* ibus) {
  GDBusConnection* connection = ibus_bus_get_connection(ibus);
  if (!connection) {
    DVLOG(1) << "IBusConnection is null";
    return NULL;
  }
  // This function does not issue an IBus IPC.
  IBusInputContext* context = ibus_input_context_get_input_context(
      input_context_path.c_str(), connection);
  if (!context)
    DVLOG(1) << "IBusInputContext is null: " << input_context_path;
  return context;
}

// Returns true if |prop| has children.
bool PropertyHasChildren(IBusProperty* prop) {
  return prop && ibus_property_get_sub_props(prop) &&
      ibus_prop_list_get(ibus_property_get_sub_props(prop), 0);
}

// This function is called by and FlattenProperty() and converts IBus
// representation of a property, |ibus_prop|, to our own and push_back the
// result to |out_prop_list|. This function returns true on success, and
// returns false if sanity checks for |ibus_prop| fail.
bool ConvertProperty(IBusProperty* ibus_prop,
                     int selection_item_id,
                     InputMethodPropertyList* out_prop_list) {
  DCHECK(ibus_prop);
  DCHECK(out_prop_list);

  const IBusPropType type = ibus_property_get_prop_type(ibus_prop);
  const IBusPropState state = ibus_property_get_state(ibus_prop);
  const IBusText* tooltip = ibus_property_get_tooltip(ibus_prop);
  const IBusText* label = ibus_property_get_label(ibus_prop);
  const gchar* key = ibus_property_get_key(ibus_prop);
  DCHECK(key);

  // Sanity checks.
  const bool has_sub_props = PropertyHasChildren(ibus_prop);
  if (has_sub_props && (type != PROP_TYPE_MENU)) {
    DVLOG(1) << "The property has sub properties, "
             << "but the type of the property is not PROP_TYPE_MENU";
    return false;
  }
  if ((!has_sub_props) && (type == PROP_TYPE_MENU)) {
    // This is usually not an error. ibus-daemon sometimes sends empty props.
    DVLOG(1) << "Property list is empty";
    return false;
  }
  if (type == PROP_TYPE_SEPARATOR || type == PROP_TYPE_MENU) {
    // This is not an error, but we don't push an item for these types.
    return true;
  }

  const bool is_selection_item = (type == PROP_TYPE_RADIO);
  selection_item_id = is_selection_item ?
      selection_item_id : InputMethodProperty::kInvalidSelectionItemId;

  bool is_selection_item_checked = false;
  if (state == PROP_STATE_INCONSISTENT) {
    DVLOG(1) << "The property is in PROP_STATE_INCONSISTENT, "
             << "which is not supported.";
  } else if ((!is_selection_item) && (state == PROP_STATE_CHECKED)) {
    DVLOG(1) << "PROP_STATE_CHECKED is meaningful only if the type is "
             << "PROP_TYPE_RADIO.";
  } else {
    is_selection_item_checked = (state == PROP_STATE_CHECKED);
  }

  if (!key)
    DVLOG(1) << "key is NULL";
  if (tooltip && !tooltip->text) {
    DVLOG(1) << "tooltip is NOT NULL, but tooltip->text IS NULL: key="
             << Or(key, "");
  }
  if (label && !label->text) {
    DVLOG(1) << "label is NOT NULL, but label->text IS NULL: key="
             << Or(key, "");
  }

  // This label will be localized later.
  // See chrome/browser/chromeos/input_method/input_method_util.cc.
  std::string label_to_use = (tooltip && tooltip->text) ? tooltip->text : "";
  if (label_to_use.empty()) {
    // Usually tooltips are more descriptive than labels.
    label_to_use = (label && label->text) ? label->text : "";
  }
  if (label_to_use.empty()) {
    DVLOG(1) << "The tooltip and label are both empty. Use " << key;
    label_to_use = Or(key, "");
  }

  out_prop_list->push_back(InputMethodProperty(key,
                                               label_to_use,
                                               is_selection_item,
                                               is_selection_item_checked,
                                               selection_item_id));
  return true;
}

// Converts |ibus_prop| to |out_prop_list|. Please note that |ibus_prop|
// may or may not have children. See the comment for FlattenPropertyList
// for details. Returns true if no error is found.
bool FlattenProperty(IBusProperty* ibus_prop,
                     InputMethodPropertyList* out_prop_list) {
  DCHECK(ibus_prop);
  DCHECK(out_prop_list);

  int selection_item_id = -1;
  std::stack<std::pair<IBusProperty*, int> > prop_stack;
  prop_stack.push(std::make_pair(ibus_prop, selection_item_id));

  while (!prop_stack.empty()) {
    IBusProperty* prop = prop_stack.top().first;
    const gchar* key = ibus_property_get_key(prop);
    const int current_selection_item_id = prop_stack.top().second;
    prop_stack.pop();

    // Filter out unnecessary properties.
    if (PropertyKeyIsBlacklisted(key))
      continue;

    // Convert |prop| to InputMethodProperty and push it to |out_prop_list|.
    if (!ConvertProperty(prop, current_selection_item_id, out_prop_list))
      return false;

    // Process childrens iteratively (if any). Push all sub properties to the
    // stack.
    if (PropertyHasChildren(prop)) {
      ++selection_item_id;
      for (int i = 0;; ++i) {
        IBusProperty* sub_prop =
            ibus_prop_list_get(ibus_property_get_sub_props(prop), i);
        if (!sub_prop)
          break;
        prop_stack.push(std::make_pair(sub_prop, selection_item_id));
      }
      ++selection_item_id;
    }
  }
  std::reverse(out_prop_list->begin(), out_prop_list->end());

  return true;
}

// Converts IBus representation of a property list, |ibus_prop_list| to our
// own. This function also flatten the original list (actually it's a tree).
// Returns true if no error is found. The conversion to our own type is
// necessary since our language switcher in Chrome tree don't (or can't) know
// IBus types. Here is an example:
//
// ======================================================================
// Input:
//
// --- Item-1
//  |- Item-2
//  |- SubMenuRoot --- Item-3-1
//  |               |- Item-3-2
//  |               |- Item-3-3
//  |- Item-4
//
// (Note: Item-3-X is a selection item since they're on a sub menu.)
//
// Output:
//
// Item-1, Item-2, Item-3-1, Item-3-2, Item-3-3, Item-4
// (Note: SubMenuRoot does not appear in the output.)
// ======================================================================
bool FlattenPropertyList(IBusPropList* ibus_prop_list,
                         InputMethodPropertyList* out_prop_list) {
  DCHECK(ibus_prop_list);
  DCHECK(out_prop_list);

  IBusProperty* fake_root_prop = ibus_property_new("Dummy.Key",
                                                   PROP_TYPE_MENU,
                                                   NULL, /* label */
                                                   "", /* icon */
                                                   NULL, /* tooltip */
                                                   FALSE, /* sensitive */
                                                   FALSE, /* visible */
                                                   PROP_STATE_UNCHECKED,
                                                   ibus_prop_list);
  g_return_val_if_fail(fake_root_prop, false);
  // Increase the ref count so it won't get deleted when |fake_root_prop|
  // is deleted.
  g_object_ref(ibus_prop_list);
  const bool result = FlattenProperty(fake_root_prop, out_prop_list);
  g_object_unref(fake_root_prop);

  return result;
}

// Debug print function.
const char* PropTypeToString(int prop_type) {
  switch (static_cast<IBusPropType>(prop_type)) {
    case PROP_TYPE_NORMAL:
      return "NORMAL";
    case PROP_TYPE_TOGGLE:
      return "TOGGLE";
    case PROP_TYPE_RADIO:
      return "RADIO";
    case PROP_TYPE_MENU:
      return "MENU";
    case PROP_TYPE_SEPARATOR:
      return "SEPARATOR";
  }
  return "UNKNOWN";
}

// Debug print function.
const char* PropStateToString(int prop_state) {
  switch (static_cast<IBusPropState>(prop_state)) {
    case PROP_STATE_UNCHECKED:
      return "UNCHECKED";
    case PROP_STATE_CHECKED:
      return "CHECKED";
    case PROP_STATE_INCONSISTENT:
      return "INCONSISTENT";
  }
  return "UNKNOWN";
}

// Debug print function.
std::string Spacer(int n) {
  return std::string(n, ' ');
}

// Debug print function.
std::string PrintProp(IBusProperty *prop, int tree_level) {
  std::string PrintPropList(IBusPropList *prop_list, int tree_level);

  if (!prop)
    return "";

  const IBusPropType type = ibus_property_get_prop_type(prop);
  const IBusPropState state = ibus_property_get_state(prop);
  const IBusText* tooltip = ibus_property_get_tooltip(prop);
  const IBusText* label = ibus_property_get_label(prop);
  const gchar* key = ibus_property_get_key(prop);

  std::stringstream stream;
  stream << Spacer(tree_level) << "=========================" << std::endl;
  stream << Spacer(tree_level) << "key: " << Or(key, "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "label: "
         << ((label && label->text) ? label->text : "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "tooptip: "
         << ((tooltip && tooltip->text)
             ? tooltip->text : "<none>") << std::endl;
  stream << Spacer(tree_level) << "sensitive: "
         << (ibus_property_get_sensitive(prop) ? "YES" : "NO") << std::endl;
  stream << Spacer(tree_level) << "visible: "
         << (ibus_property_get_visible(prop) ? "YES" : "NO") << std::endl;
  stream << Spacer(tree_level) << "type: " << PropTypeToString(type)
         << std::endl;
  stream << Spacer(tree_level) << "state: " << PropStateToString(state)
         << std::endl;
  stream << Spacer(tree_level) << "sub_props: "
         << (PropertyHasChildren(prop) ? "" : "<none>") << std::endl;
  stream << PrintPropList(ibus_property_get_sub_props(prop), tree_level + 1);
  stream << Spacer(tree_level) << "=========================" << std::endl;

  return stream.str();
}

// Debug print function.
std::string PrintPropList(IBusPropList *prop_list, int tree_level) {
  if (!prop_list)
    return "";

  std::stringstream stream;
  for (int i = 0;; ++i) {
    IBusProperty* prop = ibus_prop_list_get(prop_list, i);
    if (!prop)
      break;
    stream << PrintProp(prop, tree_level);
  }
  return stream.str();
}

class IBusAddressWatcher {
 public:
  class IBusAddressFileWatcherDelegate
      : public base::files::FilePathWatcher::Delegate {
   public:
    IBusAddressFileWatcherDelegate(
        const std::string& ibus_address,
        IBusControllerImpl* controller,
        IBusAddressWatcher* watcher)
        : ibus_address_(ibus_address),
          controller_(controller),
          watcher_(watcher) {
      DCHECK(watcher);
      DCHECK(!ibus_address.empty());
    }

    virtual ~IBusAddressFileWatcherDelegate() {}

    virtual void OnFilePathChanged(const FilePath& file_path) OVERRIDE {
      if (!watcher_->IsWatching())
        return;
      bool success = content::BrowserThread::PostTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(
              &IBusControllerImpl::IBusDaemonInitializationDone,
              controller_,
              ibus_address_));
      DCHECK(success);
      watcher_->StopSoon();
    }

   private:
    // The ibus-daemon address.
    const std::string ibus_address_;
    IBusControllerImpl* controller_;
    IBusAddressWatcher* watcher_;

    DISALLOW_COPY_AND_ASSIGN(IBusAddressFileWatcherDelegate);
  };

  static void Start(const std::string& ibus_address,
                    IBusControllerImpl* controller) {
    IBusAddressWatcher* instance = IBusAddressWatcher::Get();
    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string address_file_path;
    // TODO(nona): move reading environment variables to UI thread.
    env->GetVar("IBUS_ADDRESS_FILE", &address_file_path);
    DCHECK(!address_file_path.empty());

    if (instance->IsWatching())
      instance->StopNow();
    instance->watcher_ = new base::files::FilePathWatcher;

    // The |delegate| is owned by watcher.
    IBusAddressFileWatcherDelegate* delegate =
        new IBusAddressFileWatcherDelegate(ibus_address, controller, instance);
    bool result = instance->watcher_->Watch(FilePath(address_file_path),
                                            delegate);
    DCHECK(result);
  }

  void StopNow() {
    delete watcher_;
    watcher_ = NULL;
  }

  void StopSoon() {
    if (!watcher_)
      return;
    MessageLoop::current()->DeleteSoon(FROM_HERE, watcher_);
    watcher_ = NULL;
  }

  bool IsWatching() const {
    return watcher_ != NULL;
  }

 private:
  static IBusAddressWatcher* Get() {
    static IBusAddressWatcher* instance = new IBusAddressWatcher;
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
    return instance;
  }

  // Singleton
  IBusAddressWatcher()
      :  watcher_(NULL) {}
  base::files::FilePathWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(IBusAddressWatcher);
};

}  // namespace

IBusControllerImpl::IBusControllerImpl()
    : ibus_(NULL),
      ibus_config_(NULL),
      process_handle_(base::kNullProcessHandle),
      ibus_daemon_status_(IBUS_DAEMON_STOP),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

IBusControllerImpl::~IBusControllerImpl() {
  // Disconnect signals so the handler functions will not be called with
  // |this| which is already freed.
  if (ibus_) {
    g_signal_handlers_disconnect_by_func(
        ibus_,
        reinterpret_cast<gpointer>(G_CALLBACK(BusConnectedThunk)),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_,
        reinterpret_cast<gpointer>(G_CALLBACK(BusDisconnectedThunk)),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_,
        reinterpret_cast<gpointer>(G_CALLBACK(BusNameOwnerChangedThunk)),
        this);

    // Disconnect signals for the panel service as well.
    IBusPanelService* ibus_panel_service = IBUS_PANEL_SERVICE(
        g_object_get_data(G_OBJECT(ibus_), kPanelObjectKey));
    if (ibus_panel_service) {
      g_signal_handlers_disconnect_by_func(
          ibus_panel_service,
          reinterpret_cast<gpointer>(G_CALLBACK(FocusInThunk)),
          this);
      g_signal_handlers_disconnect_by_func(
          ibus_panel_service,
          reinterpret_cast<gpointer>(G_CALLBACK(RegisterPropertiesThunk)),
          this);
      g_signal_handlers_disconnect_by_func(
          ibus_panel_service,
          reinterpret_cast<gpointer>(G_CALLBACK(UpdatePropertyThunk)),
          this);
    }
  }
}

bool IBusControllerImpl::Start() {
  MaybeInitializeIBusBus();
  if (IBusConnectionsAreAlive())
    return true;
  if (ibus_daemon_status_ == IBUS_DAEMON_STOP ||
      ibus_daemon_status_ == IBUS_DAEMON_SHUTTING_DOWN) {
    return StartIBusDaemon();
  }
  return true;
}

void IBusControllerImpl::Reset() {
  if (!IBusConnectionsAreAlive())
    return;
  IBusInputContext* context =
      GetInputContext(current_input_context_path_, ibus_);
  if (!context)
    return;
  ibus_input_context_reset(context);
}

bool IBusControllerImpl::Stop() {
  if (ibus_daemon_status_ == IBUS_DAEMON_SHUTTING_DOWN ||
      ibus_daemon_status_ == IBUS_DAEMON_STOP) {
    return true;
  }

  ibus_daemon_status_ = IBUS_DAEMON_SHUTTING_DOWN;
  // TODO(nona): Shutdown ibus-bus connection.
  if (IBusConnectionsAreAlive()) {
    // Ask IBus to exit *asynchronously*.
    ibus_bus_exit_async(ibus_,
                        FALSE  /* do not restart */,
                        -1  /* timeout */,
                        NULL  /* cancellable */,
                        NULL  /* callback */,
                        NULL  /* user_data */);
    if (ibus_config_) {
      // Release |ibus_config_| unconditionally to make sure next
      // IBusConnectionsAreAlive() call will return false.
      g_object_unref(ibus_config_);
      ibus_config_ = NULL;
    }
  } else {
    base::KillProcess(process_handle_, -1, false /* wait */);
    DVLOG(1) << "Killing ibus-daemon. PID="
             << base::GetProcId(process_handle_);
  }
  process_handle_ = base::kNullProcessHandle;
  return true;
}

bool IBusControllerImpl::ChangeInputMethod(const std::string& id) {
  if (ibus_daemon_status_ == IBUS_DAEMON_SHUTTING_DOWN ||
      ibus_daemon_status_ == IBUS_DAEMON_STOP) {
    return false;
  }

  // Sanity checks.
  DCHECK(!InputMethodUtil::IsKeyboardLayout(id));
  if (!whitelist_.InputMethodIdIsWhitelisted(id) &&
      !InputMethodUtil::IsExtensionInputMethod(id))
    return false;

  // Clear input method properties unconditionally if |id| is not equal to
  // |current_input_method_id_|.
  //
  // When switching to another input method and no text area is focused,
  // RegisterProperties signal for the new input method will NOT be sent
  // until a text area is focused. Therefore, we have to clear the old input
  // method properties here to keep the input method switcher status
  // consistent.
  //
  // When |id| and |current_input_method_id_| are the same, the properties
  // shouldn't be cleared. If we do that, something wrong happens in step #4
  // below:
  // 1. Enable "xkb:us::eng" and "mozc". Switch to "mozc".
  // 2. Focus Omnibox. IME properties for mozc are sent to Chrome.
  // 3. Switch to "xkb:us::eng". No function in this file is called.
  // 4. Switch back to "mozc". ChangeInputMethod("mozc") is called, but it's
  //    basically NOP since ibus-daemon's current IME is already "mozc".
  //    IME properties are not sent to Chrome for the same reason.
  if (id != current_input_method_id_)
    RegisterProperties(NULL, NULL);

  current_input_method_id_ = id;

  if (!IBusConnectionsAreAlive()) {
    DVLOG(1) << "ChangeInputMethod: IBus connection is not alive (yet).";
    // |id| will become usable shortly since Start() has already been called.
    // Just return true.
  } else {
    SendChangeInputMethodRequest(id);
  }

  return true;
}

bool IBusControllerImpl::ActivateInputMethodProperty(const std::string& key) {
  if (!IBusConnectionsAreAlive()) {
    DVLOG(1) << "ActivateInputMethodProperty: IBus connection is not alive";
    return false;
  }
  if (current_input_context_path_.empty()) {
    DVLOG(1) << "Input context is unknown";
    return false;
  }

  // The third parameter of ibus_input_context_property_activate() has to be
  // true when the |key| points to a radio button. false otherwise.
  bool is_radio = true;
  size_t i;
  for (i = 0; i < current_property_list_.size(); ++i) {
    if (current_property_list_[i].key == key) {
      is_radio = current_property_list_[i].is_selection_item;
      break;
    }
  }
  if (i == current_property_list_.size()) {
    DVLOG(1) << "ActivateInputMethodProperty: unknown key: " << key;
    return false;
  }

  IBusInputContext* context =
      GetInputContext(current_input_context_path_, ibus_);
  if (!context)
    return false;

  // Activate the property *asynchronously*.
  ibus_input_context_property_activate(context, key.c_str(), is_radio);

  // We don't have to call ibus_proxy_destroy(context) explicitly here,
  // i.e. we can just call g_object_unref(context), since g_object_unref can
  // trigger both dispose, which is overridden by src/ibusproxy.c, and
  // finalize functions. For details, see
  // http://library.gnome.org/devel/gobject/stable/gobject-memory.html
  g_object_unref(context);

  return true;
}

#if defined(USE_VIRTUAL_KEYBOARD)
// IBusController override.
void IBusControllerImpl::SendHandwritingStroke(
    const HandwritingStroke& stroke) {
  if (stroke.size() < 2) {
    DVLOG(1) << "Empty stroke data or a single dot is passed.";
    return;
  }

  IBusInputContext* context =
      GetInputContext(current_input_context_path_, ibus_);
  if (!context)
    return;

  const size_t raw_stroke_size = stroke.size() * 2;
  scoped_array<double> raw_stroke(new double[raw_stroke_size]);
  for (size_t n = 0; n < stroke.size(); ++n) {
    raw_stroke[n * 2] = stroke[n].first;  // x
    raw_stroke[n * 2 + 1] = stroke[n].second;  // y
  }
  ibus_input_context_process_hand_writing_event(
      context, raw_stroke.get(), raw_stroke_size);
  g_object_unref(context);
}

// IBusController override.
void IBusControllerImpl::CancelHandwriting(int n_strokes) {
  IBusInputContext* context =
      GetInputContext(current_input_context_path_, ibus_);
  if (!context)
    return;
  ibus_input_context_cancel_hand_writing(context, n_strokes);
  g_object_unref(context);
}
#endif

bool IBusControllerImpl::IBusConnectionsAreAlive() {
  return (ibus_daemon_status_ == IBUS_DAEMON_RUNNING) &&
      ibus_ && ibus_bus_is_connected(ibus_) && ibus_config_;
}

void IBusControllerImpl::MaybeRestoreConnections() {
  if (IBusConnectionsAreAlive())
    return;
  MaybeRestoreIBusConfig();
  if (IBusConnectionsAreAlive()) {
    DVLOG(1) << "ibus-daemon and ibus-memconf processes are ready.";
    ConnectPanelServiceSignals();
    SendAllInputMethodConfigs();
    if (!current_input_method_id_.empty())
      SendChangeInputMethodRequest(current_input_method_id_);
  }
}

void IBusControllerImpl::MaybeInitializeIBusBus() {
  if (ibus_)
    return;

  ibus_init();
  // Establish IBus connection between ibus-daemon to change the current input
  // method engine, properties, and so on.
  ibus_ = ibus_bus_new();
  DCHECK(ibus_);

  // Register callback functions for IBusBus signals.
  ConnectBusSignals();

  // Ask libibus to watch the NameOwnerChanged signal *asynchronously*.
  ibus_bus_set_watch_dbus_signal(ibus_, TRUE);

  if (ibus_bus_is_connected(ibus_)) {
    DVLOG(1) << "IBus connection is ready: ibus-daemon is already running?";
    BusConnected(ibus_);
  }
}

void IBusControllerImpl::MaybeRestoreIBusConfig() {
  if (!ibus_)
    return;

  // Destroy the current |ibus_config_| object. No-op if it's NULL.
  MaybeDestroyIBusConfig();

  if (ibus_config_)
    return;

  GDBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
  if (!ibus_connection) {
    DVLOG(1) << "Couldn't create an ibus config object since "
             << "IBus connection is not ready.";
    return;
  }

  const gboolean disconnected
      = g_dbus_connection_is_closed(ibus_connection);
  if (disconnected) {
    // |ibus_| object is not NULL, but the connection between ibus-daemon
    // is not yet established. In this case, we don't create |ibus_config_|
    // object.
    DVLOG(1) << "Couldn't create an ibus config object since "
             << "IBus connection is closed.";
    return;
  }
  // If memconf is not successfully started yet, ibus_config_new() will
  // return NULL. Otherwise, it returns a transfer-none and non-floating
  // object. ibus_config_new() sometimes issues a D-Bus *synchronous* IPC
  // to check if the org.freedesktop.IBus.Config service is available.
  ibus_config_ = ibus_config_new(ibus_connection,
                                 NULL /* do not cancel the operation */,
                                 NULL /* do not get error information */);
  if (!ibus_config_) {
    DVLOG(1) << "ibus_config_new() failed. ibus-memconf is not ready?";
    return;
  }

  // TODO(yusukes): g_object_weak_ref might be better since it allows
  // libcros to detect the delivery of the "destroy" glib signal the
  // |ibus_config_| object.
  g_object_ref(ibus_config_);
  DVLOG(1) << "ibus_config_ is ready.";
}

void IBusControllerImpl::MaybeDestroyIBusConfig() {
  if (!ibus_) {
    DVLOG(1) << "MaybeDestroyIBusConfig: ibus_ is NULL";
    return;
  }
  if (ibus_config_ && !ibus_bus_is_connected(ibus_)) {
    g_object_unref(ibus_config_);
    ibus_config_ = NULL;
  }
}

void IBusControllerImpl::SendChangeInputMethodRequest(const std::string& id) {
  // Change the global engine *asynchronously*.
  ibus_bus_set_global_engine_async(ibus_,
                                   id.c_str(),
                                   -1,  // use the default ibus timeout
                                   NULL,  // cancellable
                                   NULL,  // callback
                                   NULL);  // user_data
}

void IBusControllerImpl::SendAllInputMethodConfigs() {
  DCHECK(IBusConnectionsAreAlive());

  InputMethodConfigRequests::const_iterator iter =
      current_config_values_.begin();
  for (; iter != current_config_values_.end(); ++iter) {
    SetInputMethodConfigInternal(iter->first, iter->second);
  }
}

bool IBusControllerImpl::SetInputMethodConfigInternal(
    const ConfigKeyType& key,
    const InputMethodConfigValue& value) {
  if (!IBusConnectionsAreAlive())
    return true;

  // Convert the type of |value| from our structure to GVariant.
  GVariant* variant = NULL;
  switch (value.type) {
    case InputMethodConfigValue::kValueTypeString:
      variant = g_variant_new_string(value.string_value.c_str());
      break;
    case InputMethodConfigValue::kValueTypeInt:
      variant = g_variant_new_int32(value.int_value);
      break;
    case InputMethodConfigValue::kValueTypeBool:
      variant = g_variant_new_boolean(value.bool_value);
      break;
    case InputMethodConfigValue::kValueTypeStringList:
      GVariantBuilder variant_builder;
      g_variant_builder_init(&variant_builder, G_VARIANT_TYPE("as"));
      const size_t size = value.string_list_value.size();
      // |size| could be 0 for some special configurations such as IBus hotkeys.
      for (size_t i = 0; i < size; ++i) {
        g_variant_builder_add(&variant_builder,
                              "s",
                              value.string_list_value[i].c_str());
      }
      variant = g_variant_builder_end(&variant_builder);
      break;
  }

  if (!variant) {
    DVLOG(1) << "SendInputMethodConfig: unknown value.type";
    return false;
  }
  DCHECK(g_variant_is_floating(variant));
  DCHECK(ibus_config_);

  // Set an ibus configuration value *asynchronously*.
  ibus_config_set_value_async(ibus_config_,
                              key.first.c_str(),
                              key.second.c_str(),
                              variant,
                              -1,  // use the default ibus timeout
                              NULL,  // cancellable
                              SetInputMethodConfigCallback,
                              g_object_ref(ibus_config_));

  // Since |variant| is floating, ibus_config_set_value_async consumes
  // (takes ownership of) the variable.
  return true;
}

void IBusControllerImpl::ConnectBusSignals() {
  if (!ibus_)
    return;

  // We use g_signal_connect_after here since the callback should be called
  // *after* the IBusBusDisconnectedCallback in chromeos_input_method_ui.cc
  // is called. chromeos_input_method_ui.cc attaches the panel service object
  // to |ibus_|, and the callback in this file use the attached object.
  g_signal_connect_after(ibus_,
                         "connected",
                         G_CALLBACK(BusConnectedThunk),
                         this);

  g_signal_connect(ibus_,
                   "disconnected",
                   G_CALLBACK(BusDisconnectedThunk),
                   this);

  g_signal_connect(ibus_,
                   "name-owner-changed",
                   G_CALLBACK(BusNameOwnerChangedThunk),
                   this);
}

void IBusControllerImpl::ConnectPanelServiceSignals() {
  if (!ibus_)
    return;

  IBusPanelService* ibus_panel_service = IBUS_PANEL_SERVICE(
      g_object_get_data(G_OBJECT(ibus_), kPanelObjectKey));
  if (!ibus_panel_service) {
    DVLOG(1) << "IBusPanelService is NOT available.";
    return;
  }
  // We don't _ref() or _weak_ref() the panel service object, since we're not
  // interested in the life time of the object.

  g_signal_connect(ibus_panel_service,
                   "focus-in",
                   G_CALLBACK(FocusInThunk),
                   this);
  g_signal_connect(ibus_panel_service,
                   "register-properties",
                   G_CALLBACK(RegisterPropertiesThunk),
                   this);
  g_signal_connect(ibus_panel_service,
                   "update-property",
                   G_CALLBACK(UpdatePropertyThunk),
                   this);
}

void IBusControllerImpl::BusConnected(IBusBus* bus) {
  DVLOG(1) << "IBus connection is established.";
  MaybeRestoreConnections();
}

void IBusControllerImpl::BusDisconnected(IBusBus* bus) {
  DVLOG(1) << "IBus connection is terminated.";
  // ibus-daemon might be terminated. Since |ibus_| object will automatically
  // connect to the daemon if it restarts, we don't have to set NULL on ibus_.
  // Call MaybeDestroyIBusConfig() to set |ibus_config_| to NULL temporarily.
  MaybeDestroyIBusConfig();
}

void IBusControllerImpl::BusNameOwnerChanged(IBusBus* bus,
                                             const gchar* name,
                                             const gchar* old_name,
                                             const gchar* new_name) {
  DCHECK(name);
  DCHECK(old_name);
  DCHECK(new_name);

  if (name != std::string("org.freedesktop.IBus.Config")) {
    // Not a signal for ibus-memconf.
    return;
  }

  const std::string empty_string;
  if (old_name != empty_string || new_name == empty_string) {
    // ibus-memconf died?
    DVLOG(1) << "Unexpected name owner change: name=" << name
             << ", old_name=" << old_name << ", new_name=" << new_name;
    // TODO(yusukes): it might be nice to set |ibus_config_| to NULL and call
    // a new callback function like OnDisconnect() here to allow Chrome to
    // recover all input method configurations when ibus-memconf is
    // automatically restarted by ibus-daemon. Though ibus-memconf is pretty
    // stable and unlikely crashes.
    return;
  }
  DVLOG(1) << "IBus config daemon is started. Recovering ibus_config_";

  // Try to recover |ibus_config_|. If the |ibus_config_| object is
  // successfully created, |OnConnectionChange| will be called to
  // notify Chrome that IBus is ready.
  MaybeRestoreConnections();
}

void IBusControllerImpl::FocusIn(IBusPanelService* panel,
                                 const gchar* input_context_path) {
  if (!input_context_path)
    DVLOG(1) << "NULL context passed";
  else
    DVLOG(1) << "FocusIn: " << input_context_path;
  // Remember the current ic path.
  current_input_context_path_ = Or(input_context_path, "");
}

void IBusControllerImpl::RegisterProperties(IBusPanelService* panel,
                                            IBusPropList* ibus_prop_list) {
  // Note: |panel| can be NULL. See ChangeInputMethod().
  DVLOG(1) << "RegisterProperties" << (ibus_prop_list ? "" : " (clear)");

  current_property_list_.clear();
  if (ibus_prop_list) {
    // You can call
    //   DVLOG(1) << "\n" << PrintPropList(ibus_prop_list, 0);
    // here to dump |ibus_prop_list|.
    if (!FlattenPropertyList(ibus_prop_list, &current_property_list_)) {
      // Clear properties on errors.
      current_property_list_.clear();
    }
  }
  FOR_EACH_OBSERVER(Observer, observers_, PropertyChanged());
}

void IBusControllerImpl::UpdateProperty(IBusPanelService* panel,
                                        IBusProperty* ibus_prop) {
  DVLOG(1) << "UpdateProperty";
  DCHECK(ibus_prop);

  // You can call
  //   DVLOG(1) << "\n" << PrintProp(ibus_prop, 0);
  // here to dump |ibus_prop|.

  InputMethodPropertyList prop_list;  // our representation.
  if (!FlattenProperty(ibus_prop, &prop_list)) {
    // Don't update the UI on errors.
    DVLOG(1) << "Malformed properties are detected";
    return;
  }

  // Notify the change.
  if (!prop_list.empty()) {
    for (size_t i = 0; i < prop_list.size(); ++i) {
      FindAndUpdateProperty(prop_list[i], &current_property_list_);
    }
    FOR_EACH_OBSERVER(Observer, observers_, PropertyChanged());
  }
}

bool IBusControllerImpl::StartIBusDaemon() {
  if (ibus_daemon_status_ == IBUS_DAEMON_INITIALIZING ||
      ibus_daemon_status_ == IBUS_DAEMON_RUNNING) {
    DVLOG(1) << "MaybeLaunchIBusDaemon: ibus-daemon is already running.";
    return false;
  }

  ibus_daemon_status_ = IBUS_DAEMON_INITIALIZING;
  ibus_daemon_address_ = base::StringPrintf(
      "unix:abstract=ibus-%d",
      base::RandInt(0, std::numeric_limits<int>::max()));

  // Set up ibus-daemon address file watcher before launching ibus-daemon,
  // because if watcher starts after ibus-daemon, we may miss the ibus
  // connection initialization.
  bool success = content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&IBusAddressWatcher::Start,
                 ibus_daemon_address_,
                 base::Unretained(this)),
      base::Bind(&IBusControllerImpl::LaunchIBusDaemon,
                 weak_ptr_factory_.GetWeakPtr(),
                 ibus_daemon_address_));
  DCHECK(success);
  return true;
}

void IBusControllerImpl::LaunchIBusDaemon(const std::string& ibus_address) {
  DCHECK_EQ(base::kNullProcessHandle, process_handle_);
  static const char kIBusDaemonPath[] = "/usr/bin/ibus-daemon";
  // TODO(zork): Send output to /var/log/ibus.log
  const std::string ibus_daemon_command_line =
      base::StringPrintf("%s --panel=disable --cache=none --restart --replace"
                         " --address=%s",
                         kIBusDaemonPath,
                         ibus_address.c_str());
  if (!LaunchProcess(ibus_daemon_command_line,
                     &process_handle_,
                     reinterpret_cast<GChildWatchFunc>(OnIBusDaemonExit))) {
    DVLOG(1) << "Failed to launch " << ibus_daemon_command_line;
  }
}

bool IBusControllerImpl::LaunchProcess(const std::string& command_line,
                                       base::ProcessHandle* process_handle,
                                       GChildWatchFunc watch_func) {
  std::vector<std::string> argv;
  base::ProcessHandle handle = base::kNullProcessHandle;

  base::SplitString(command_line, ' ', &argv);

  if (!base::LaunchProcess(argv, base::LaunchOptions(), &handle)) {
    DVLOG(1) << "Could not launch: " << command_line;
    return false;
  }

  // g_child_watch_add is necessary to prevent the process from becoming a
  // zombie.
  // TODO(yusukes): port g_child_watch_add to base/process_utils_posix.cc.
  const base::ProcessId pid = base::GetProcId(handle);
  g_child_watch_add(pid, watch_func, this);

  *process_handle = handle;
  DVLOG(1) << command_line << "is started. PID=" << pid;
  return true;
}

// static
void IBusControllerImpl::IBusDaemonInitializationDone(
    IBusControllerImpl* controller,
    const std::string& ibus_address) {
  if (controller->ibus_daemon_address_ != ibus_address)
    return;

  if (controller->ibus_daemon_status_ != IBUS_DAEMON_INITIALIZING) {
    // Stop() or OnIBusDaemonExit() has already been called.
    return;
  }
  chromeos::DBusThreadManager::Get()->InitIBusBus(ibus_address);
  controller->ibus_daemon_status_ = IBUS_DAEMON_RUNNING;
  VLOG(1) << "The ibus-daemon initialization is done.";
}

// static
void IBusControllerImpl::SetInputMethodConfigCallback(GObject* source_object,
                                                      GAsyncResult* res,
                                                      gpointer user_data) {
  IBusConfig* config = IBUS_CONFIG(user_data);
  g_return_if_fail(config);

  GError* error = NULL;
  const gboolean result =
      ibus_config_set_value_async_finish(config, res, &error);

  if (!result) {
    std::string message = "(unknown error)";
    if (error && error->message) {
      message = error->message;
    }
    DVLOG(1) << "ibus_config_set_value_async failed: " << message;
  }

  if (error)
    g_error_free(error);
  g_object_unref(config);
}

// static
void IBusControllerImpl::OnIBusDaemonExit(GPid pid,
                                          gint status,
                                          IBusControllerImpl* controller) {
  if (controller->process_handle_ != base::kNullProcessHandle) {
    if (base::GetProcId(controller->process_handle_) == pid) {
      // ibus-daemon crashed.
      // TODO(nona): Shutdown ibus-bus connection.
      controller->process_handle_ = base::kNullProcessHandle;
    } else {
      // This condition is as follows.
      // 1. Called Stop (process_handle_ becomes null)
      // 2. Called LaunchProcess (process_handle_ becomes new instance)
      // 3. Callbacked OnIBusDaemonExit for old instance and reach here.
      // In this case, we should not reset process_handle_ as null, and do not
      // re-launch ibus-daemon.
      return;
    }
  }

  const IBusDaemonStatus on_exit_state = controller->ibus_daemon_status_;
  controller->ibus_daemon_status_ = IBUS_DAEMON_STOP;
  if (on_exit_state == IBUS_DAEMON_SHUTTING_DOWN) {
    // Normal exitting, so do nothing.
    return;
  }

  LOG(ERROR) << "The ibus-daemon crashed. Re-launching...";
  controller->StartIBusDaemon();
}
#endif  // defined(HAVE_IBUS)

// static
bool IBusControllerImpl::FindAndUpdatePropertyForTesting(
    const chromeos::input_method::InputMethodProperty& new_prop,
    chromeos::input_method::InputMethodPropertyList* prop_list) {
  return FindAndUpdateProperty(new_prop, prop_list);
}

}  // namespace input_method
}  // namespace chromeos
