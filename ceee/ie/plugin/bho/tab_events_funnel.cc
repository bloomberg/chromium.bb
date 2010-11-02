// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Events from whereever through the Broker.

#include "ceee/ie/plugin/bho/tab_events_funnel.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "ceee/ie/common/constants.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"

namespace ext_event_names = extension_event_names;
namespace keys = extension_tabs_module_constants;

HRESULT TabEventsFunnel::OnCreated(HWND tab_handle, BSTR url, bool complete) {
  DictionaryValue tab_values;
  tab_values.SetInteger(keys::kIdKey, reinterpret_cast<int>(tab_handle));
  tab_values.SetString(keys::kUrlKey, url);
  tab_values.SetString(keys::kStatusKey, complete ? keys::kStatusValueComplete :
                                                    keys::kStatusValueLoading);
  return SendEvent(ext_event_names::kOnTabCreated, tab_values);
}

HRESULT TabEventsFunnel::OnMoved(HWND tab_handle, int window_id, int from_index,
                                 int to_index) {
  // For tab moves, the args are an array of two values, the tab id as an int
  // and then a dictionary with window id, from and to indexes.
  ListValue tab_moved_args;
  tab_moved_args.Append(Value::CreateIntegerValue(
      reinterpret_cast<int>(tab_handle)));
  DictionaryValue* dict = new DictionaryValue;
  dict->SetInteger(keys::kWindowIdKey, window_id);
  dict->SetInteger(keys::kFromIndexKey, from_index);
  dict->SetInteger(keys::kFromIndexKey, to_index);
  tab_moved_args.Append(dict);
  return SendEvent(ext_event_names::kOnTabMoved, tab_moved_args);
}

HRESULT TabEventsFunnel::OnRemoved(HWND tab_handle) {
  scoped_ptr<Value> args(Value::CreateIntegerValue(
      reinterpret_cast<int>(tab_handle)));
  return SendEvent(ext_event_names::kOnTabRemoved, *args.get());
}

HRESULT TabEventsFunnel::OnSelectionChanged(HWND tab_handle, int window_id) {
  // For tab selection changes, the args are an array of two values, the tab id
  // as an int and then a dictionary with only the window id in it.
  ListValue tab_selection_changed_args;
  tab_selection_changed_args.Append(Value::CreateIntegerValue(
      reinterpret_cast<int>(tab_handle)));
  DictionaryValue* dict = new DictionaryValue;
  dict->SetInteger(keys::kWindowIdKey, window_id);
  tab_selection_changed_args.Append(dict);
  return SendEvent(ext_event_names::kOnTabSelectionChanged,
                   tab_selection_changed_args);
}

HRESULT TabEventsFunnel::OnUpdated(HWND tab_handle, BSTR url,
                                   READYSTATE ready_state) {
  // For tab updates, the args are an array of two values, the tab id as an int
  // and then a dictionary with an optional url field as well as a mandatory
  // status string value.
  ListValue tab_update_args;
  tab_update_args.Append(Value::CreateIntegerValue(
      reinterpret_cast<int>(tab_handle)));
  DictionaryValue* dict = new DictionaryValue;
  if (url != NULL)
    dict->SetString(keys::kUrlKey, url);
  dict->SetString(keys::kStatusKey, (ready_state == READYSTATE_COMPLETE) ?
      keys::kStatusValueComplete : keys::kStatusValueLoading);
  tab_update_args.Append(dict);
  return SendEvent(ext_event_names::kOnTabUpdated, tab_update_args);
}

HRESULT TabEventsFunnel::OnTabUnmapped(HWND tab_handle, int tab_id) {
  ListValue tab_unmapped_args;
  tab_unmapped_args.Append(Value::CreateIntegerValue(
      reinterpret_cast<int>(tab_handle)));
  tab_unmapped_args.Append(Value::CreateIntegerValue(tab_id));
  return SendEvent(ceee_event_names::kCeeeOnTabUnmapped, tab_unmapped_args);
}
