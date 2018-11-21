// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps {

// Wraps two apps::mojom::AppPtr's, a prior state and a delta on top of that
// state. The state is conceptually the "sum" of all of the previous deltas,
// with "addition" or "merging" simply being that the most recent version of
// each field "wins".
//
// Almost all of an AppPtr's fields are optional. For example, if an app's name
// hasn't changed, then a delta doesn't necessarily have to contain a copy of
// the name, as the prior state should already contain it.
//
// The combination of the two (state and delta) can answer questions such as:
//  - What is the app's name? If the delta knows, that's the answer. Otherwise,
//    ask the state.
//  - Is the app ready to launch (i.e. installed)? Likewise, if the delta says
//    yes or no, that's the answer. Otherwise, the delta says "unknown", so ask
//    the state.
//  - Was the app *freshly* installed (i.e. it previously wasn't installed, but
//    now is)? Has its readiness changed? Check if the delta says "installed"
//    and the state says either "uninstalled" or unknown.
//
// An AppUpdate is read-only once constructed. All of its fields and methods
// are const. The constructor caller must guarantee that the AppPtr references
// remain valid for the lifetime of the AppUpdate.
//
// See //chrome/services/app_service/README.md for more details.
class AppUpdate {
 public:
  // Modifies state by copying over all of delta's known fields: those fields
  // whose values aren't "unknown".
  static void Merge(apps::mojom::App* state, const apps::mojom::AppPtr& delta);

  AppUpdate(const apps::mojom::AppPtr& state, const apps::mojom::AppPtr& delta);

  apps::mojom::AppType AppType() const;

  const std::string& AppId() const;

  apps::mojom::Readiness Readiness() const;
  bool ReadinessChanged() const;

  const std::string& Name() const;
  bool NameChanged() const;

  apps::mojom::OptionalBool ShowInLauncher() const;
  bool ShowInLauncherChanged() const;

 private:
  const apps::mojom::AppPtr& state_;
  const apps::mojom::AppPtr& delta_;

  DISALLOW_COPY_AND_ASSIGN(AppUpdate);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_
