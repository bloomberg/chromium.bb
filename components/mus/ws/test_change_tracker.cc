// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/test_change_tracker.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/mus/public/cpp/util.h"
#include "mojo/common/common_type_converters.h"

using mojo::Array;
using mojo::ViewDataPtr;
using mojo::String;

namespace mus {

std::string ViewIdToString(Id id) {
  return (id == 0) ? "null"
                   : base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

namespace {

std::string RectToString(const mojo::Rect& rect) {
  return base::StringPrintf("%d,%d %dx%d", rect.x, rect.y, rect.width,
                            rect.height);
}

std::string DirectionToString(mojo::OrderDirection direction) {
  return direction == mojo::ORDER_DIRECTION_ABOVE ? "above" : "below";
}

std::string ChangeToDescription1(const Change& change) {
  switch (change.type) {
    case CHANGE_TYPE_EMBED:
      return "OnEmbed";

    case CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED:
      return base::StringPrintf("OnEmbeddedAppDisconnected view=%s",
                                ViewIdToString(change.view_id).c_str());

    case CHANGE_TYPE_UNEMBED:
      return "OnUnembed";

    case CHANGE_TYPE_NODE_BOUNDS_CHANGED:
      return base::StringPrintf(
          "BoundsChanged view=%s old_bounds=%s new_bounds=%s",
          ViewIdToString(change.view_id).c_str(),
          RectToString(change.bounds).c_str(),
          RectToString(change.bounds2).c_str());

    case CHANGE_TYPE_NODE_VIEWPORT_METRICS_CHANGED:
      // TODO(sky): Not implemented.
      return "ViewportMetricsChanged";

    case CHANGE_TYPE_NODE_HIERARCHY_CHANGED:
      return base::StringPrintf(
          "HierarchyChanged view=%s new_parent=%s old_parent=%s",
          ViewIdToString(change.view_id).c_str(),
          ViewIdToString(change.view_id2).c_str(),
          ViewIdToString(change.view_id3).c_str());

    case CHANGE_TYPE_NODE_REORDERED:
      return base::StringPrintf("Reordered view=%s relative=%s direction=%s",
                                ViewIdToString(change.view_id).c_str(),
                                ViewIdToString(change.view_id2).c_str(),
                                DirectionToString(change.direction).c_str());

    case CHANGE_TYPE_NODE_DELETED:
      return base::StringPrintf("ViewDeleted view=%s",
                                ViewIdToString(change.view_id).c_str());

    case CHANGE_TYPE_NODE_VISIBILITY_CHANGED:
      return base::StringPrintf("VisibilityChanged view=%s visible=%s",
                                ViewIdToString(change.view_id).c_str(),
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED:
      return base::StringPrintf("DrawnStateChanged view=%s drawn=%s",
                                ViewIdToString(change.view_id).c_str(),
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_INPUT_EVENT:
      return base::StringPrintf("InputEvent view=%s event_action=%d",
                                ViewIdToString(change.view_id).c_str(),
                                change.event_action);

    case CHANGE_TYPE_PROPERTY_CHANGED:
      return base::StringPrintf("PropertyChanged view=%s key=%s value=%s",
                                ViewIdToString(change.view_id).c_str(),
                                change.property_key.c_str(),
                                change.property_value.c_str());

    case CHANGE_TYPE_DELEGATE_EMBED:
      return base::StringPrintf("DelegateEmbed url=%s",
                                change.embed_url.data());

    case CHANGE_TYPE_FOCUSED:
      return base::StringPrintf("Focused id=%s",
                                ViewIdToString(change.view_id).c_str());
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

std::string SingleViewDescription(const std::vector<TestView>& views) {
  if (views.size() != 1u)
    return "more than one changes and expected only one";
  return views[0].ToString();
}

std::string ChangeViewDescription(const std::vector<Change>& changes) {
  if (changes.size() != 1)
    return std::string();
  std::vector<std::string> view_strings(changes[0].views.size());
  for (size_t i = 0; i < changes[0].views.size(); ++i)
    view_strings[i] = "[" + changes[0].views[i].ToString() + "]";
  return base::JoinString(view_strings, ",");
}

TestView ViewDataToTestView(const ViewDataPtr& data) {
  TestView view;
  view.parent_id = data->parent_id;
  view.view_id = data->view_id;
  view.visible = data->visible;
  view.drawn = data->drawn;
  view.properties =
      data->properties.To<std::map<std::string, std::vector<uint8_t>>>();
  return view;
}

void ViewDatasToTestViews(const Array<ViewDataPtr>& data,
                          std::vector<TestView>* test_views) {
  for (size_t i = 0; i < data.size(); ++i)
    test_views->push_back(ViewDataToTestView(data[i]));
}

Change::Change()
    : type(CHANGE_TYPE_EMBED),
      connection_id(0),
      view_id(0),
      view_id2(0),
      view_id3(0),
      event_action(0),
      direction(mojo::ORDER_DIRECTION_ABOVE),
      bool_value(false) {}

Change::~Change() {}

TestChangeTracker::TestChangeTracker() : delegate_(NULL) {}

TestChangeTracker::~TestChangeTracker() {}

void TestChangeTracker::OnEmbed(ConnectionSpecificId connection_id,
                                ViewDataPtr root) {
  Change change;
  change.type = CHANGE_TYPE_EMBED;
  change.connection_id = connection_id;
  change.views.push_back(ViewDataToTestView(root));
  AddChange(change);
}

void TestChangeTracker::OnEmbeddedAppDisconnected(Id view_id) {
  Change change;
  change.type = CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED;
  change.view_id = view_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowBoundsChanged(Id view_id,
                                              mojo::RectPtr old_bounds,
                                              mojo::RectPtr new_bounds) {
  Change change;
  change.type = CHANGE_TYPE_NODE_BOUNDS_CHANGED;
  change.view_id = view_id;
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

void TestChangeTracker::OnUnembed() {
  Change change;
  change.type = CHANGE_TYPE_UNEMBED;
  AddChange(change);
}

void TestChangeTracker::OnWindowViewportMetricsChanged(
    mojo::ViewportMetricsPtr old_metrics,
    mojo::ViewportMetricsPtr new_metrics) {
  Change change;
  change.type = CHANGE_TYPE_NODE_VIEWPORT_METRICS_CHANGED;
  // NOT IMPLEMENTED
  AddChange(change);
}

void TestChangeTracker::OnWindowHierarchyChanged(Id view_id,
                                                 Id new_parent_id,
                                                 Id old_parent_id,
                                                 Array<ViewDataPtr> views) {
  Change change;
  change.type = CHANGE_TYPE_NODE_HIERARCHY_CHANGED;
  change.view_id = view_id;
  change.view_id2 = new_parent_id;
  change.view_id3 = old_parent_id;
  ViewDatasToTestViews(views, &change.views);
  AddChange(change);
}

void TestChangeTracker::OnWindowReordered(Id view_id,
                                          Id relative_view_id,
                                          mojo::OrderDirection direction) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REORDERED;
  change.view_id = view_id;
  change.view_id2 = relative_view_id;
  change.direction = direction;
  AddChange(change);
}

void TestChangeTracker::OnWindowDeleted(Id view_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DELETED;
  change.view_id = view_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowVisibilityChanged(Id view_id, bool visible) {
  Change change;
  change.type = CHANGE_TYPE_NODE_VISIBILITY_CHANGED;
  change.view_id = view_id;
  change.bool_value = visible;
  AddChange(change);
}

void TestChangeTracker::OnWindowDrawnStateChanged(Id view_id, bool drawn) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED;
  change.view_id = view_id;
  change.bool_value = drawn;
  AddChange(change);
}

void TestChangeTracker::OnWindowInputEvent(Id view_id, mojo::EventPtr event) {
  Change change;
  change.type = CHANGE_TYPE_INPUT_EVENT;
  change.view_id = view_id;
  change.event_action = event->action;
  AddChange(change);
}

void TestChangeTracker::OnWindowSharedPropertyChanged(Id view_id,
                                                      String name,
                                                      Array<uint8_t> data) {
  Change change;
  change.type = CHANGE_TYPE_PROPERTY_CHANGED;
  change.view_id = view_id;
  change.property_key = name;
  if (data.is_null())
    change.property_value = "NULL";
  else
    change.property_value = data.To<std::string>();
  AddChange(change);
}

void TestChangeTracker::OnWindowFocused(Id view_id) {
  Change change;
  change.type = CHANGE_TYPE_FOCUSED;
  change.view_id = view_id;
  AddChange(change);
}

void TestChangeTracker::DelegateEmbed(const String& url) {
  Change change;
  change.type = CHANGE_TYPE_DELEGATE_EMBED;
  change.embed_url = url;
  AddChange(change);
}

void TestChangeTracker::AddChange(const Change& change) {
  changes_.push_back(change);
  if (delegate_)
    delegate_->OnChangeAdded();
}

TestView::TestView() {}

TestView::~TestView() {}

std::string TestView::ToString() const {
  return base::StringPrintf("view=%s parent=%s",
                            ViewIdToString(view_id).c_str(),
                            ViewIdToString(parent_id).c_str());
}

std::string TestView::ToString2() const {
  return base::StringPrintf(
      "view=%s parent=%s visible=%s drawn=%s", ViewIdToString(view_id).c_str(),
      ViewIdToString(parent_id).c_str(), visible ? "true" : "false",
      drawn ? "true" : "false");
}

}  // namespace mus
