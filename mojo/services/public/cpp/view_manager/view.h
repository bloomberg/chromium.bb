// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_

#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager_constants.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

class SkBitmap;

namespace mojo {

class ServiceProviderImpl;
class View;
class ViewManager;
class ViewObserver;

// Views are owned by the ViewManager.
// TODO(beng): Right now, you'll have to implement a ViewObserver to track
//             destruction and NULL any pointers you have.
//             Investigate some kind of smart pointer or weak pointer for these.
class View {
 public:
  typedef std::vector<View*> Children;

  static View* Create(ViewManager* view_manager);

  // Destroys this view and all its children.
  void Destroy();

  // Configuration.
  Id id() const { return id_; }

  // Geometric disposition.
  const gfx::Rect& bounds() { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  // Visibility.
  void SetVisible(bool value);
  // TODO(sky): add accessor.

  // Observation.
  void AddObserver(ViewObserver* observer);
  void RemoveObserver(ViewObserver* observer);

  // Tree.
  View* parent() { return parent_; }
  const View* parent() const { return parent_; }
  const Children& children() const { return children_; }

  void AddChild(View* child);
  void RemoveChild(View* child);

  void Reorder(View* relative, OrderDirection direction);
  void MoveToFront();
  void MoveToBack();

  bool Contains(View* child) const;

  View* GetChildById(Id id);

  // TODO(beng): temporary only.
  void SetContents(const SkBitmap& contents);
  void SetColor(SkColor color);

  // Focus.
  void SetFocus();

  // Embedding.
  void Embed(const String& url);
  scoped_ptr<ServiceProvider> Embed(
      const String& url,
      scoped_ptr<ServiceProviderImpl> exported_services);

 protected:
  // This class is subclassed only by test classes that provide a public ctor.
  View();
  ~View();

 private:
  friend class ViewPrivate;
  friend class ViewManagerClientImpl;

  explicit View(ViewManager* manager);

  void LocalDestroy();
  void LocalAddChild(View* child);
  void LocalRemoveChild(View* child);
  // Returns true if the order actually changed.
  bool LocalReorder(View* relative, OrderDirection direction);
  void LocalSetBounds(const gfx::Rect& old_bounds, const gfx::Rect& new_bounds);

  ViewManager* manager_;
  Id id_;
  View* parent_;
  Children children_;

  ObserverList<ViewObserver> observers_;

  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_H_
