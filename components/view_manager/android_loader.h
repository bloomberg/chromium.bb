// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_ANDROID_LOADER_H_
#define COMPONENTS_VIEW_MANAGER_ANDROID_LOADER_H_

#include "mojo/shell/application_loader.h"

namespace mojo {
class ApplicationImpl;
}

namespace view_manager {

class AndroidLoader : public mojo::shell::ApplicationLoader {
public:
  AndroidLoader();
  ~AndroidLoader();

 private:
  // Overridden from mojo::shell::ApplicationLoader:
  void Load(
      const GURL& url,
      mojo::InterfaceRequest<mojo::Application> application_request) override;

  scoped_ptr<mojo::ApplicationImpl> app_;

  DISALLOW_COPY_AND_ASSIGN(AndroidLoader);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_ANDROID_LOADER_H_
