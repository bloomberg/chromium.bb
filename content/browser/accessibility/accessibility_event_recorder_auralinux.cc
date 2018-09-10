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
  void AddGlobalListeners();
  void RemoveGlobalListeners();

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

void AccessibilityEventRecorderAuraLinux::AddGlobalListeners() {
  static constexpr size_t kArraySize = 2;
  constexpr std::array<const char*, kArraySize> events{
      {"ATK:AtkObject:state-change", "ATK:AtkObject:focus-event"}};

  for (size_t i = 0; i < kArraySize; i++) {
    unsigned int id = atk_add_global_event_listener(OnEventReceived, events[i]);
    if (!id)
      LOG(FATAL) << "atk_add_global_event_listener failed for " << events[i];

    listener_ids_.push_back(id);
  }
}

void AccessibilityEventRecorderAuraLinux::RemoveGlobalListeners() {
  for (const auto& id : listener_ids_)
    atk_remove_global_event_listener(id);

  listener_ids_.clear();
}

void AccessibilityEventRecorderAuraLinux::ProcessEvent(const char* event,
                                                       unsigned int n_params,
                                                       const GValue* params) {
  // If we don't have a root object, it means the tree is being destroyed.
  if (!manager_->GetRoot()) {
    RemoveGlobalListeners();
    return;
  }

  std::string log = base::ToUpperASCII(event);
  if (std::string(event).find("state-change") != std::string::npos) {
    log += ":" + base::ToUpperASCII(g_value_get_string(&params[1]));
    log += base::StringPrintf(":%s", g_strdup_value_contents(&params[2]));
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
    if (atk_state_set_contains_state(state_set, state_type)) {
      states += " " + base::ToUpperASCII(atk_state_type_get_name(state_type));
    }
  }
  states = base::CollapseWhitespaceASCII(states, false);
  base::ReplaceChars(states, " ", ",", &states);
  log += base::StringPrintf(" %s", states.c_str());

  OnEvent(log);
}

}  // namespace content
