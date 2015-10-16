// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TEST_CHANGE_TRACKER_H_
#define COMPONENTS_MUS_WS_TEST_CHANGE_TRACKER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "ui/mojo/geometry/geometry.mojom.h"

namespace mus {

enum ChangeType {
  CHANGE_TYPE_EMBED,
  CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED,
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

// TODO(sky): consider nuking and converting directly to WindowData.
struct TestView {
  TestView();
  ~TestView();

  // Returns a string description of this.
  std::string ToString() const;

  // Returns a string description that includes visible and drawn.
  std::string ToString2() const;

  Id parent_id;
  Id window_id;
  bool visible;
  bool drawn;
  std::map<std::string, std::vector<uint8_t>> properties;
};

// Tracks a call to ViewTreeClient. See the individual functions for the fields
// that are used.
struct Change {
  Change();
  ~Change();

  ChangeType type;
  ConnectionSpecificId connection_id;
  std::vector<TestView> views;
  Id window_id;
  Id window_id2;
  Id window_id3;
  mojo::Rect bounds;
  mojo::Rect bounds2;
  int32_t event_action;
  mojo::String embed_url;
  mojom::OrderDirection direction;
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

// Converts WindowDatas to TestViews.
void WindowDatasToTestViews(const mojo::Array<mojom::WindowDataPtr>& data,
                            std::vector<TestView>* test_views);

// TestChangeTracker is used to record ViewTreeClient functions. It notifies
// a delegate any time a change is added.
class TestChangeTracker {
 public:
  // Used to notify the delegate when a change is added. A change corresponds to
  // a single ViewTreeClient function.
  class Delegate {
   public:
    virtual void OnChangeAdded() = 0;

   protected:
    virtual ~Delegate() {}
  };

  TestChangeTracker();
  ~TestChangeTracker();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  std::vector<Change>* changes() { return &changes_; }

  // Each of these functions generate a Change. There is one per
  // ViewTreeClient function.
  void OnEmbed(ConnectionSpecificId connection_id, mojom::WindowDataPtr root);
  void OnEmbeddedAppDisconnected(Id window_id);
  void OnUnembed();
  void OnWindowBoundsChanged(Id window_id,
                             mojo::RectPtr old_bounds,
                             mojo::RectPtr new_bounds);
  void OnWindowViewportMetricsChanged(mojom::ViewportMetricsPtr old_bounds,
                                      mojom::ViewportMetricsPtr new_bounds);
  void OnWindowHierarchyChanged(Id window_id,
                                Id new_parent_id,
                                Id old_parent_id,
                                mojo::Array<mojom::WindowDataPtr> views);
  void OnWindowReordered(Id window_id,
                         Id relative_window_id,
                         mojom::OrderDirection direction);
  void OnWindowDeleted(Id window_id);
  void OnWindowVisibilityChanged(Id window_id, bool visible);
  void OnWindowDrawnStateChanged(Id window_id, bool drawn);
  void OnWindowInputEvent(Id window_id, mojo::EventPtr event);
  void OnWindowSharedPropertyChanged(Id window_id,
                                     mojo::String name,
                                     mojo::Array<uint8_t> data);
  void OnWindowFocused(Id window_id);
  void DelegateEmbed(const mojo::String& url);

 private:
  void AddChange(const Change& change);

  Delegate* delegate_;
  std::vector<Change> changes_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeTracker);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TEST_CHANGE_TRACKER_H_
