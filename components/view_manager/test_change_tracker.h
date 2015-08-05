// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_
#define COMPONENTS_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "components/view_manager/public/cpp/types.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "ui/mojo/geometry/geometry.mojom.h"

namespace view_manager {

enum ChangeType {
  CHANGE_TYPE_EMBED,
  CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED,
  CHANGE_TYPE_EMBED_FOR_DESCENDANT,
  CHANGE_TYPE_UNEMBED,
  // TODO(sky): nuke NODE.
  CHANGE_TYPE_NODE_BOUNDS_CHANGED,
  CHANGE_TYPE_NODE_VIEWPORT_METRICS_CHANGED,
  CHANGE_TYPE_NODE_HIERARCHY_CHANGED,
  CHANGE_TYPE_NODE_REORDERED,
  CHANGE_TYPE_NODE_VISIBILITY_CHANGED,
  CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED,
  CHANGE_TYPE_NODE_DELETED,
  CHANGE_TYPE_INPUT_EVENT,
  CHANGE_TYPE_PROPERTY_CHANGED,
  CHANGE_TYPE_DELEGATE_EMBED,
  CHANGE_TYPE_FOCUSED,
};

// TODO(sky): consider nuking and converting directly to ViewData.
struct TestView {
  TestView();
  ~TestView();

  // Returns a string description of this.
  std::string ToString() const;

  // Returns a string description that includes visible and drawn.
  std::string ToString2() const;

  mojo::Id parent_id;
  mojo::Id view_id;
  bool visible;
  bool drawn;
  std::map<std::string, std::vector<uint8_t>> properties;
};

// Tracks a call to ViewManagerClient. See the individual functions for the
// fields that are used.
struct Change {
  Change();
  ~Change();

  ChangeType type;
  mojo::ConnectionSpecificId connection_id;
  std::vector<TestView> views;
  mojo::Id view_id;
  mojo::Id view_id2;
  mojo::Id view_id3;
  mojo::Rect bounds;
  mojo::Rect bounds2;
  int32_t event_action;
  mojo::String embed_url;
  mojo::OrderDirection direction;
  bool bool_value;
  std::string property_key;
  std::string property_value;
};

// Converts Changes to string descriptions.
std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes);

// Convenience for returning the description of the first item in |changes|.
// Returns an empty string if |changes| has something other than one entry.
std::string SingleChangeToDescription(const std::vector<Change>& changes);

// Convenience for returning the description of the first item in |views|.
// Returns an empty string if |views| has something other than one entry.
std::string SingleViewDescription(const std::vector<TestView>& views);

// Returns a string description of |changes[0].views|. Returns an empty string
// if change.size() != 1.
std::string ChangeViewDescription(const std::vector<Change>& changes);

// Converts ViewDatas to TestViews.
void ViewDatasToTestViews(const mojo::Array<mojo::ViewDataPtr>& data,
                          std::vector<TestView>* test_views);

// TestChangeTracker is used to record ViewManagerClient functions. It notifies
// a delegate any time a change is added.
class TestChangeTracker {
 public:
  // Used to notify the delegate when a change is added. A change corresponds to
  // a single ViewManagerClient function.
  class Delegate {
   public:
    virtual void OnChangeAdded() = 0;

   protected:
    virtual ~Delegate() {}
  };

  TestChangeTracker();
  ~TestChangeTracker();

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

  std::vector<Change>* changes() { return &changes_; }

  // Each of these functions generate a Change. There is one per
  // ViewManagerClient function.
  void OnEmbed(mojo::ConnectionSpecificId connection_id,
               mojo::ViewDataPtr root);
  void OnEmbedForDescendant(mojo::Id view_id);
  void OnEmbeddedAppDisconnected(mojo::Id view_id);
  void OnUnembed();
  void OnViewBoundsChanged(mojo::Id view_id,
                           mojo::RectPtr old_bounds,
                           mojo::RectPtr new_bounds);
  void OnViewViewportMetricsChanged(mojo::ViewportMetricsPtr old_bounds,
                                    mojo::ViewportMetricsPtr new_bounds);
  void OnViewHierarchyChanged(mojo::Id view_id,
                              mojo::Id new_parent_id,
                              mojo::Id old_parent_id,
                              mojo::Array<mojo::ViewDataPtr> views);
  void OnViewReordered(mojo::Id view_id,
                       mojo::Id relative_view_id,
                       mojo::OrderDirection direction);
  void OnViewDeleted(mojo::Id view_id);
  void OnViewVisibilityChanged(mojo::Id view_id, bool visible);
  void OnViewDrawnStateChanged(mojo::Id view_id, bool drawn);
  void OnViewInputEvent(mojo::Id view_id, mojo::EventPtr event);
  void OnViewSharedPropertyChanged(mojo::Id view_id,
                                   mojo::String name,
                                   mojo::Array<uint8_t> data);
  void OnViewFocused(mojo::Id view_id);
  void DelegateEmbed(const mojo::String& url);

 private:
  void AddChange(const Change& change);

  Delegate* delegate_;
  std::vector<Change> changes_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeTracker);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_
