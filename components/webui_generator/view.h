// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WUG_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_WUG_VIEW_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/webui_generator/export.h"

namespace base {
class DictionaryValue;
}

namespace webui_generator {

class ViewModel;

/**
 * Base block of Web UI (in terms of Web UI Generator). Every View instance is
 * associated with single ViewModel instance which provides a context for the
 * view. Views are hierarchical. Every child of view has an unique id.
 */
class WUG_EXPORT View {
 public:
  explicit View(const std::string& id);
  virtual ~View();

  // Should be called before using the view. This method creates children (using
  // CreateAndAddChildren() method) and a view-model (using CreateViewModel()
  // method). Then it recursively calls Init() for the children. When
  // all the children and the view-model are ready, OnReady() method is called.
  // Overridden methods should call the base implementation.
  virtual void Init();

  // Root view is a view without parent.
  // Root view should have id equal to "WUG_ROOT".
  bool IsRootView() const;

  ViewModel* GetViewModel() const;

  // Called by view-model when it is ready.
  void OnViewModelReady();

  const std::string& id() const { return id_; }

  // Every view has an unique path in a view hierarchy which is:
  //  a) |id| for the root view;
  //  b) concatenation of parent's path, $-sign and |id| for not-root views.
  const std::string& path() const { return path_; }

  // Returns a child with a given |id|. Returns |nullptr| if such child doesn't
  // exist.
  View* GetChild(const std::string& id) const;

  // Called by view-model when it changes the context.
  virtual void OnContextChanged(const base::DictionaryValue& diff) = 0;

 protected:
  // Called when view is ready, which means view-model and all children are
  // ready. Overridden methods should call the base implementation.
  virtual void OnReady();

  // Forwards context changes stored in |diff| to view-model.
  void UpdateContext(const base::DictionaryValue& diff);

  // Forwards |event| to view-model.
  void HandleEvent(const std::string& event);

  bool ready() const { return ready_; }

  base::ScopedPtrHashMap<std::string, View>& children() { return children_; }

  // Adds |child| to the list of children of |this|. Can be called only from
  // CreateAndAddChildren() override.
  void AddChild(View* child);

  virtual std::string GetType() = 0;

  // Implementation should create an instance of view-model for this view.
  virtual ViewModel* CreateViewModel() = 0;

  // Implementation should create and add children to |this| view. This method
  // is an only place where it is allowed to add children.
  virtual void CreateAndAddChildren() = 0;

 private:
  // Called by |child| view, when it is ready.
  void OnChildReady(View* child);

  // Called when all the children created by CreatedAndAddChildren() are ready.
  void OnChildrenReady();

  void set_parent(View* parent) { parent_ = parent; }

  View* parent_;
  std::string id_;
  std::string path_;

  // Number of child views that are ready.
  int ready_children_;

  bool view_model_ready_;
  bool ready_;
  base::ScopedPtrHashMap<std::string, View> children_;
  scoped_ptr<ViewModel> view_model_;
  base::WeakPtrFactory<View> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace webui_generator

#endif  // CHROME_BROWSER_UI_WEBUI_WUG_VIEW_H_
