// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"

class SkBitmap;

namespace mojo {
namespace view_manager {

class ViewManager;
class ViewObserver;
class ViewTreeNode;

// Views are owned by the ViewManager.
class View {
 public:
  static View* Create(ViewManager* manager);

  void Destroy();

  TransportViewId id() const { return id_; }
  ViewTreeNode* node() { return node_; }

  void AddObserver(ViewObserver* observer);
  void RemoveObserver(ViewObserver* observer);

  void SetContents(const SkBitmap& contents);

 private:
  friend class ViewPrivate;

  explicit View(ViewManager* manager);
  View();
  ~View();

  void LocalDestroy();

  TransportViewId id_;
  ViewTreeNode* node_;
  ViewManager* manager_;

  ObserverList<ViewObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_
