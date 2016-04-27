// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_IMPL_H_
#define COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_IMPL_H_

#include "ash/link_handler_model.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/intent_helper.mojom.h"

namespace arc {

class LinkHandlerModelImpl : public ash::LinkHandlerModel {
 public:
  LinkHandlerModelImpl();
  ~LinkHandlerModelImpl() override;

  // ash::LinkHandlerModel overrides:
  void AddObserver(Observer* observer) override;
  void OpenLinkWithHandler(const GURL& url, uint32_t handler_id) override;

  // Starts retrieving handler information for the |url| and returns true.
  // Returns false when the information cannot be retrieved. In that case,
  // the caller should delete |this| object.
  bool Init(const GURL& url);

 private:
  mojom::IntentHelperInstance* GetIntentHelper();
  void OnUrlHandlerList(mojo::Array<mojom::UrlHandlerInfoPtr> handlers);
  void NotifyObserver();

  base::ObserverList<Observer> observer_list_;
  mojo::Array<mojom::UrlHandlerInfoPtr> handlers_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<LinkHandlerModelImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LinkHandlerModelImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_IMPL_H_
