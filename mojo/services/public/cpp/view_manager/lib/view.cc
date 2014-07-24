// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view.h"

#include "mojo/services/public/cpp/view_manager/lib/view_manager_client_impl.h"
#include "mojo/services/public/cpp/view_manager/lib/view_private.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "ui/gfx/canvas.h"

namespace mojo {

// static
View* View::Create(ViewManager* manager) {
  View* view = new View(manager);
  static_cast<ViewManagerClientImpl*>(manager)->AddView(view);
  return view;
}

void View::Destroy() {
  if (manager_)
    static_cast<ViewManagerClientImpl*>(manager_)->DestroyView(id_);
  LocalDestroy();
}

void View::AddObserver(ViewObserver* observer) {
  observers_.AddObserver(observer);
}

void View::RemoveObserver(ViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void View::SetContents(const SkBitmap& contents) {
  if (manager_) {
    static_cast<ViewManagerClientImpl*>(manager_)->SetViewContents(id_,
                                                                   contents);
  }
}

void View::SetColor(SkColor color) {
  gfx::Canvas canvas(node_->bounds().size(), 1.0f, true);
  canvas.DrawColor(color);
  SetContents(skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(true));
}

View::View(ViewManager* manager)
    : id_(static_cast<ViewManagerClientImpl*>(manager)->CreateView()),
      node_(NULL),
      manager_(manager) {}

View::View()
    : id_(static_cast<Id>(-1)),
      node_(NULL),
      manager_(NULL) {}

View::~View() {
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDestroying(this));
  // TODO(beng): It'd be better to do this via a destruction observer in the
  //             ViewManagerClientImpl.
  if (manager_)
    static_cast<ViewManagerClientImpl*>(manager_)->RemoveView(id_);

  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDestroyed(this));
}

void View::LocalDestroy() {
  delete this;
}

}  // namespace mojo
