// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view.h"

#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_private.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"

namespace mojo {
namespace view_manager {

namespace {
class ScopedDestructionNotifier {
 public:
  explicit ScopedDestructionNotifier(View* view)
      : view_(view) {
    FOR_EACH_OBSERVER(
        ViewObserver,
        *ViewPrivate(view_).observers(),
        OnViewDestroy(view_, ViewObserver::DISPOSITION_CHANGING));
  }
  ~ScopedDestructionNotifier() {
    FOR_EACH_OBSERVER(
        ViewObserver,
        *ViewPrivate(view_).observers(),
        OnViewDestroy(view_, ViewObserver::DISPOSITION_CHANGED));
  }

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDestructionNotifier);
};
}  // namespace

// static
View* View::Create(ViewManager* manager) {
  View* view = new View(manager);
  ViewManagerPrivate(manager).AddView(view->id(), view);
  return view;
}

void View::Destroy() {
  if (manager_)
    ViewManagerPrivate(manager_).synchronizer()->DestroyView(id_);
  LocalDestroy();
}

void View::AddObserver(ViewObserver* observer) {
  observers_.AddObserver(observer);
}

void View::RemoveObserver(ViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void View::SetContents(const SkBitmap& contents) {
  if (manager_)
    ViewManagerPrivate(manager_).synchronizer()->SetViewContents(id_, contents);
}

View::View(ViewManager* manager)
    : id_(ViewManagerPrivate(manager).synchronizer()->CreateView()),
      node_(NULL),
      manager_(manager) {}

View::View()
    : id_(-1),
      node_(NULL),
      manager_(NULL) {}

View::~View() {
  ScopedDestructionNotifier notifier(this);
  if (manager_)
    ViewManagerPrivate(manager_).RemoveView(id_);
}

void View::LocalDestroy() {
  delete this;
}

}  // namespace view_manager
}  // namespace mojo
