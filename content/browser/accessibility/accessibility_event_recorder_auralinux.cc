// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder.h"

#include <atk/atk.h>
#include <atk/atkutil.h>

#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "content/browser/accessibility/browser_accessibility_auralinux.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 16, 0)
#define ATK_216
#endif

namespace content {

class AccessibilityEventRecorderAuraLinux : public AccessibilityEventRecorder {
 public:
  explicit AccessibilityEventRecorderAuraLinux(
      BrowserAccessibilityManager* manager,
      base::ProcessId pid);
  ~AccessibilityEventRecorderAuraLinux() override;

  void ProcessEvent(const char* event,
                    unsigned int n_params,
                    const GValue* params);

 private:
  void AddGlobalListener(const char* event_name);
  void AddGlobalListeners();
  void RemoveGlobalListeners();
  bool IncludeState(AtkStateType state_type);

  std::vector<unsigned int> listener_ids_;
};

// static
gboolean OnEventReceived(GSignalInvocationHint* hint,
                         unsigned int n_params,
                         const GValue* params,
                         gpointer data) {
  GSignalQuery query;
  g_signal_query(hint->signal_id, &query);

  static_cast<AccessibilityEventRecorderAuraLinux&>(
      AccessibilityEventRecorder::GetInstance())
      .ProcessEvent(query.signal_name, n_params, params);
  return true;
}

// static
AccessibilityEventRecorder& AccessibilityEventRecorder::GetInstance(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid) {
  static base::NoDestructor<AccessibilityEventRecorderAuraLinux> instance(
      manager, pid);
  return *instance;
}

AccessibilityEventRecorderAuraLinux::AccessibilityEventRecorderAuraLinux(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid)
    : AccessibilityEventRecorder(manager, pid) {
  AddGlobalListeners();
}

AccessibilityEventRecorderAuraLinux::~AccessibilityEventRecorderAuraLinux() {}

void AccessibilityEventRecorderAuraLinux::AddGlobalListener(
    const char* event_name) {
  unsigned id = atk_add_global_event_listener(OnEventReceived, event_name);
  if (!id)
    LOG(FATAL) << "atk_add_global_event_listener failed for " << event_name;

  listener_ids_.push_back(id);
}

void AccessibilityEventRecorderAuraLinux::AddGlobalListeners() {
  GObject* gobject = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr, nullptr));
  g_object_unref(atk_no_op_object_new(gobject));
  g_object_unref(gobject);

  AddGlobalListener("ATK:AtkObject:state-change");
  AddGlobalListener("ATK:AtkObject:focus-event");
  AddGlobalListener("ATK:AtkObject:property-change");
}

void AccessibilityEventRecorderAuraLinux::RemoveGlobalListeners() {
  for (const auto& id : listener_ids_)
    atk_remove_global_event_listener(id);

  listener_ids_.clear();
}

// Pruning states which are not supported on older bots makes it possible to
// run the events tests in more environments.
bool AccessibilityEventRecorderAuraLinux::IncludeState(
    AtkStateType state_type) {
  switch (state_type) {
#if defined(ATK_216)
    case ATK_STATE_CHECKABLE:
    case ATK_STATE_HAS_POPUP:
    case ATK_STATE_READ_ONLY:
      return false;
#endif
    case ATK_STATE_LAST_DEFINED:
      return false;
    default:
      return true;
  }
}

void AccessibilityEventRecorderAuraLinux::ProcessEvent(const char* event,
                                                       unsigned int n_params,
                                                       const GValue* params) {
  // If we don't have a root object, it means the tree is being destroyed.
  if (!manager_->GetRoot()) {
    RemoveGlobalListeners();
    return;
  }

  std::string log;
  if (std::string(event).find("property-change") != std::string::npos) {
    DCHECK_GE(n_params, 2u);
    AtkPropertyValues* property_values =
        static_cast<AtkPropertyValues*>(g_value_get_pointer(&params[1]));

    if (g_strcmp0(property_values->property_name, "accessible-value"))
      return;

    log += "VALUE-CHANGED:";
    log +=
        base::NumberToString(g_value_get_double(&property_values->new_value));
  } else {
    log += base::ToUpperASCII(event);
    if (std::string(event).find("state-change") != std::string::npos) {
      log += ":" + base::ToUpperASCII(g_value_get_string(&params[1]));
      log += base::StringPrintf(":%s", g_strdup_value_contents(&params[2]));
    }
  }

  AtkObject* obj = ATK_OBJECT(g_value_get_object(&params[0]));
  std::string role = atk_role_get_name(atk_object_get_role(obj));
  base::ReplaceChars(role, " ", "_", &role);
  log += base::StringPrintf(" role=ROLE_%s", base::ToUpperASCII(role).c_str());
  log += base::StringPrintf(" name='%s'", atk_object_get_name(obj));

  std::string states = "";
  AtkStateSet* state_set = atk_object_ref_state_set(obj);
  for (int i = ATK_STATE_INVALID; i < ATK_STATE_LAST_DEFINED; i++) {
    AtkStateType state_type = static_cast<AtkStateType>(i);
    if (atk_state_set_contains_state(state_set, state_type) &&
        IncludeState(state_type)) {
      states += " " + base::ToUpperASCII(atk_state_type_get_name(state_type));
    }
  }
  states = base::CollapseWhitespaceASCII(states, false);
  base::ReplaceChars(states, " ", ",", &states);
  log += base::StringPrintf(" %s", states.c_str());

  OnEvent(log);
}

}  // namespace content
