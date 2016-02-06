// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_ANDROID_LOADER_H_
#define COMPONENTS_MUS_ANDROID_LOADER_H_

#include "base/macros.h"
#include "mojo/shell/application_loader.h"

namespace mojo {
class ApplicationImpl;
}

namespace mus {

class AndroidLoader : public mojo::shell::ApplicationLoader {
 public:
  AndroidLoader();
  ~AndroidLoader();

 private:
  // Overridden from mojo::shell::ApplicationLoader:
  void Load(
      const GURL& url,
      mojo::InterfaceRequest<mojo::shell::mojom::Application> request) override;

  scoped_ptr<mojo::ApplicationImpl> app_;

  DISALLOW_COPY_AND_ASSIGN(AndroidLoader);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_ANDROID_LOADER_H_
