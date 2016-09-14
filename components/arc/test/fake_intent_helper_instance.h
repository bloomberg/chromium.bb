// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_INTENT_HELPER_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_INTENT_HELPER_INSTANCE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/test/fake_arc_bridge_instance.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class FakeIntentHelperInstance : public mojom::IntentHelperInstance {
 public:
  FakeIntentHelperInstance();

  class Broadcast {
   public:
    Broadcast(const mojo::String& action,
              const mojo::String& package_name,
              const mojo::String& cls,
              const mojo::String& extras);

    ~Broadcast();

    Broadcast(const Broadcast& broadcast);

    std::string action;
    std::string package_name;
    std::string cls;
    std::string extras;
  };

  void clear_broadcasts() { broadcasts_.clear(); }

  const std::vector<Broadcast>& broadcasts() const { return broadcasts_; }

  // mojom::HelpIntentInstance:
  ~FakeIntentHelperInstance() override;

  void AddPreferredPackage(const mojo::String& package_name) override;

  void HandleUrl(const mojo::String& url,
                 const mojo::String& package_name) override;

  void HandleUrlList(mojo::Array<mojom::UrlWithMimeTypePtr> urls,
                     mojom::ActivityNamePtr activity,
                     mojom::ActionType action) override;

  void HandleUrlListDeprecated(mojo::Array<mojom::UrlWithMimeTypePtr> urls,
                               const mojo::String& package_name,
                               mojom::ActionType action) override;

  void Init(mojom::IntentHelperHostPtr host_ptr) override;

  void RequestActivityIcons(
      mojo::Array<mojom::ActivityNamePtr> activities,
      ::arc::mojom::ScaleFactor scale_factor,
      const RequestActivityIconsCallback& callback) override;

  void RequestUrlHandlerList(
      const mojo::String& url,
      const RequestUrlHandlerListCallback& callback) override;

  void RequestUrlListHandlerList(
      mojo::Array<mojom::UrlWithMimeTypePtr> urls,
      const RequestUrlListHandlerListCallback& callback) override;

  void SendBroadcast(const mojo::String& action,
                     const mojo::String& package_name,
                     const mojo::String& cls,
                     const mojo::String& extras) override;

 private:
  std::vector<Broadcast> broadcasts_;

  DISALLOW_COPY_AND_ASSIGN(FakeIntentHelperInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_
