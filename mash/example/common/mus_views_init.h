// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_EXAMPLE_COMMON_MUS_VIEWS_INIT_H_
#define MASH_EXAMPLE_COMMON_MUS_VIEWS_INIT_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/views_delegate.h"

namespace mojo {
class ApplicationImpl;
}

namespace views {
class AuraInit;
}

// Does the necessary setup to use mus, views and the example wm.
class MUSViewsInit : public views::ViewsDelegate,
                     public mus::WindowTreeDelegate {
 public:
  explicit MUSViewsInit(mojo::ApplicationImpl* app);
  ~MUSViewsInit() override;

 private:
  mus::Window* NewWindow();

  // views::ViewsDelegate:
  views::NativeWidget* CreateNativeWidget(
      views::internal::NativeWidgetDelegate* delegate) override;
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override;

  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;
#if defined(OS_WIN)
  HICON GetSmallWindowIcon() const override;
#endif

  mojo::ApplicationImpl* app_;
  scoped_ptr<views::AuraInit> aura_init_;
  mus::mojom::WindowManagerPtr window_manager_;

  DISALLOW_COPY_AND_ASSIGN(MUSViewsInit);
};

#endif  // MASH_EXAMPLE_COMMON_MUS_VIEWS_INIT_H_
