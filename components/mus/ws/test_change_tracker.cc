// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/test_change_tracker.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/mus/common/util.h"
#include "mojo/common/common_type_converters.h"

using mojo::Array;
using mojo::String;

namespace mus {

namespace ws {

std::string WindowIdToString(Id id) {
  return (id == 0) ? "null"
                   : base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

namespace {

std::string RectToString(const mojo::Rect& rect) {
  return base::StringPrintf("%d,%d %dx%d", rect.x, rect.y, rect.width,
                            rect.height);
}

std::string DirectionToString(mojom::OrderDirection direction) {
  return direction == mojom::OrderDirection::ABOVE ? "above" : "below";
}

std::string ChangeToDescription1(const Change& change) {
  switch (change.type) {
    case CHANGE_TYPE_EMBED:
      return "OnEmbed";

    case CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED:
      return base::StringPrintf("OnEmbeddedAppDisconnected window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_UNEMBED:
      return base::StringPrintf("OnUnembed window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_NODE_ADD_TRANSIENT_WINDOW:
      return base::StringPrintf("AddTransientWindow parent = %s child = %s",
                                WindowIdToString(change.window_id).c_str(),
                                WindowIdToString(change.window_id2).c_str());

    case CHANGE_TYPE_NODE_BOUNDS_CHANGED:
      return base::StringPrintf(
          "BoundsChanged window=%s old_bounds=%s new_bounds=%s",
          WindowIdToString(change.window_id).c_str(),
          RectToString(change.bounds).c_str(),
          RectToString(change.bounds2).c_str());

    case CHANGE_TYPE_NODE_VIEWPORT_METRICS_CHANGED:
      // TODO(sky): Not implemented.
      return "ViewportMetricsChanged";

    case CHANGE_TYPE_NODE_HIERARCHY_CHANGED:
      return base::StringPrintf(
          "HierarchyChanged window=%s new_parent=%s old_parent=%s",
          WindowIdToString(change.window_id).c_str(),
          WindowIdToString(change.window_id2).c_str(),
          WindowIdToString(change.window_id3).c_str());

    case CHANGE_TYPE_NODE_REMOVE_TRANSIENT_WINDOW_FROM_PARENT:
      return base::StringPrintf(
          "RemoveTransientWindowFromParent parent = %s child = %s",
          WindowIdToString(change.window_id).c_str(),
          WindowIdToString(change.window_id2).c_str());

    case CHANGE_TYPE_NODE_REORDERED:
      return base::StringPrintf("Reordered window=%s relative=%s direction=%s",
                                WindowIdToString(change.window_id).c_str(),
                                WindowIdToString(change.window_id2).c_str(),
                                DirectionToString(change.direction).c_str());

    case CHANGE_TYPE_NODE_DELETED:
      return base::StringPrintf("WindowDeleted window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_NODE_VISIBILITY_CHANGED:
      return base::StringPrintf("VisibilityChanged window=%s visible=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED:
      return base::StringPrintf("DrawnStateChanged window=%s drawn=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_INPUT_EVENT:
      return base::StringPrintf("InputEvent window=%s event_action=%d",
                                WindowIdToString(change.window_id).c_str(),
                                change.event_action);

    case CHANGE_TYPE_PROPERTY_CHANGED:
      return base::StringPrintf("PropertyChanged window=%s key=%s value=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.property_key.c_str(),
                                change.property_value.c_str());

    case CHANGE_TYPE_FOCUSED:
      return base::StringPrintf("Focused id=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_CURSOR_CHANGED:
      return base::StringPrintf("CursorChanged id=%s cursor_id=%d",
                                WindowIdToString(change.window_id).c_str(),
                                change.cursor_id);
    case CHANGE_TYPE_ON_CHANGE_COMPLETED:
      return base::StringPrintf("ChangeCompleted id=%d sucess=%s",
                                change.change_id,
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_ON_TOP_LEVEL_CREATED:
      return base::StringPrintf("TopLevelCreated id=%d window_id=%s",
                                change.change_id,
                                WindowIdToString(change.window_id).c_str());
  }
  return std::string();
}

}  // namespace

std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes) {
  std::vector<std::string> strings(changes.size());
  for (size_t i = 0; i < changes.size(); ++i)
    strings[i] = ChangeToDescription1(changes[i]);
  return strings;
}

std::string SingleChangeToDescription(const std::vector<Change>& changes) {
  std::string result;
  for (auto& change : changes) {
    if (!result.empty())
      result += "\n";
    result += ChangeToDescription1(change);
  }
  return result;
}

std::string SingleWindowDescription(const std::vector<TestWindow>& windows) {
  if (windows.empty())
    return "no windows";
  std::string result;
  for (const TestWindow& window : windows)
    result += window.ToString();
  return result;
}

std::string ChangeWindowDescription(const std::vector<Change>& changes) {
  if (changes.size() != 1)
    return std::string();
  std::vector<std::string> window_strings(changes[0].windows.size());
  for (size_t i = 0; i < changes[0].windows.size(); ++i)
    window_strings[i] = "[" + changes[0].windows[i].ToString() + "]";
  return base::JoinString(window_strings, ",");
}

TestWindow WindowDataToTestWindow(const mojom::WindowDataPtr& data) {
  TestWindow window;
  window.parent_id = data->parent_id;
  window.window_id = data->window_id;
  window.visible = data->visible;
  window.drawn = data->drawn;
  window.properties =
      data->properties.To<std::map<std::string, std::vector<uint8_t>>>();
  return window;
}

void WindowDatasToTestWindows(const Array<mojom::WindowDataPtr>& data,
                              std::vector<TestWindow>* test_windows) {
  for (size_t i = 0; i < data.size(); ++i)
    test_windows->push_back(WindowDataToTestWindow(data[i]));
}

Change::Change()
    : type(CHANGE_TYPE_EMBED),
      connection_id(0),
      window_id(0),
      window_id2(0),
      window_id3(0),
      event_action(0),
      direction(mojom::OrderDirection::ABOVE),
      bool_value(false),
      change_id(0u) {}

Change::~Change() {}

TestChangeTracker::TestChangeTracker() : delegate_(NULL) {}

TestChangeTracker::~TestChangeTracker() {}

void TestChangeTracker::OnEmbed(ConnectionSpecificId connection_id,
                                mojom::WindowDataPtr root) {
  Change change;
  change.type = CHANGE_TYPE_EMBED;
  change.connection_id = connection_id;
  change.windows.push_back(WindowDataToTestWindow(root));
  AddChange(change);
}

void TestChangeTracker::OnEmbeddedAppDisconnected(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowBoundsChanged(Id window_id,
                                              mojo::RectPtr old_bounds,
                                              mojo::RectPtr new_bounds) {
  Change change;
  change.type = CHANGE_TYPE_NODE_BOUNDS_CHANGED;
  change.window_id = window_id;
  change.bounds.x = old_bounds->x;
  change.bounds.y = old_bounds->y;
  change.bounds.width = old_bounds->width;
  change.bounds.height = old_bounds->height;
  change.bounds2.x = new_bounds->x;
  change.bounds2.y = new_bounds->y;
  change.bounds2.width = new_bounds->width;
  change.bounds2.height = new_bounds->height;
  AddChange(change);
}

void TestChangeTracker::OnUnembed(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_UNEMBED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnTransientWindowAdded(Id window_id,
                                               Id transient_window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_ADD_TRANSIENT_WINDOW;
  change.window_id = window_id;
  change.window_id2 = transient_window_id;
  AddChange(change);
}

void TestChangeTracker::OnTransientWindowRemoved(Id window_id,
                                                 Id transient_window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REMOVE_TRANSIENT_WINDOW_FROM_PARENT;
  change.window_id = window_id;
  change.window_id2 = transient_window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowViewportMetricsChanged(
    mojom::ViewportMetricsPtr old_metrics,
    mojom::ViewportMetricsPtr new_metrics) {
  Change change;
  change.type = CHANGE_TYPE_NODE_VIEWPORT_METRICS_CHANGED;
  // NOT IMPLEMENTED
  AddChange(change);
}

void TestChangeTracker::OnWindowHierarchyChanged(
    Id window_id,
    Id new_parent_id,
    Id old_parent_id,
    Array<mojom::WindowDataPtr> windows) {
  Change change;
  change.type = CHANGE_TYPE_NODE_HIERARCHY_CHANGED;
  change.window_id = window_id;
  change.window_id2 = new_parent_id;
  change.window_id3 = old_parent_id;
  WindowDatasToTestWindows(windows, &change.windows);
  AddChange(change);
}

void TestChangeTracker::OnWindowReordered(Id window_id,
                                          Id relative_window_id,
                                          mojom::OrderDirection direction) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REORDERED;
  change.window_id = window_id;
  change.window_id2 = relative_window_id;
  change.direction = direction;
  AddChange(change);
}

void TestChangeTracker::OnWindowDeleted(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DELETED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowVisibilityChanged(Id window_id, bool visible) {
  Change change;
  change.type = CHANGE_TYPE_NODE_VISIBILITY_CHANGED;
  change.window_id = window_id;
  change.bool_value = visible;
  AddChange(change);
}

void TestChangeTracker::OnWindowDrawnStateChanged(Id window_id, bool drawn) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED;
  change.window_id = window_id;
  change.bool_value = drawn;
  AddChange(change);
}

void TestChangeTracker::OnWindowInputEvent(Id window_id,
                                           mojom::EventPtr event) {
  Change change;
  change.type = CHANGE_TYPE_INPUT_EVENT;
  change.window_id = window_id;
  change.event_action = static_cast<int32_t>(event->action);
  AddChange(change);
}

void TestChangeTracker::OnWindowSharedPropertyChanged(Id window_id,
                                                      String name,
                                                      Array<uint8_t> data) {
  Change change;
  change.type = CHANGE_TYPE_PROPERTY_CHANGED;
  change.window_id = window_id;
  change.property_key = name;
  if (data.is_null())
    change.property_value = "NULL";
  else
    change.property_value = data.To<std::string>();
  AddChange(change);
}

void TestChangeTracker::OnWindowFocused(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_FOCUSED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowPredefinedCursorChanged(
    Id window_id,
    mojom::Cursor cursor_id) {
  Change change;
  change.type = CHANGE_TYPE_CURSOR_CHANGED;
  change.window_id = window_id;
  change.cursor_id = static_cast<int32_t>(cursor_id);
  AddChange(change);
}

void TestChangeTracker::OnChangeCompleted(uint32_t change_id, bool success) {
  Change change;
  change.type = CHANGE_TYPE_ON_CHANGE_COMPLETED;
  change.change_id = change_id;
  change.bool_value = success;
  AddChange(change);
}

void TestChangeTracker::OnTopLevelCreated(uint32_t change_id,
                                          mojom::WindowDataPtr window_data) {
  Change change;
  change.type = CHANGE_TYPE_ON_TOP_LEVEL_CREATED;
  change.change_id = change_id;
  change.window_id = window_data->window_id;
  AddChange(change);
}

void TestChangeTracker::AddChange(const Change& change) {
  changes_.push_back(change);
  if (delegate_)
    delegate_->OnChangeAdded();
}

TestWindow::TestWindow() {}

TestWindow::~TestWindow() {}

std::string TestWindow::ToString() const {
  return base::StringPrintf("window=%s parent=%s",
                            WindowIdToString(window_id).c_str(),
                            WindowIdToString(parent_id).c_str());
}

std::string TestWindow::ToString2() const {
  return base::StringPrintf(
      "window=%s parent=%s visible=%s drawn=%s",
      WindowIdToString(window_id).c_str(), WindowIdToString(parent_id).c_str(),
      visible ? "true" : "false", drawn ? "true" : "false");
}

}  // namespace ws

}  // namespace mus
