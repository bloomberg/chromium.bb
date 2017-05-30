// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_INTERFACE_H_
#define COMPONENTS_UKM_UKM_INTERFACE_H_

#include "components/ukm/public/interfaces/ukm_interface.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace ukm {

class UkmRecorder;

class UkmInterface : public mojom::UkmRecorderInterface {
 public:
  explicit UkmInterface(UkmRecorder* ukm_recorder);
  ~UkmInterface() override;

  static void Create(UkmRecorder* ukm_recorder,
                     const service_manager::BindSourceInfo& source_info,
                     mojom::UkmRecorderInterfaceRequest request);

 private:
  // ukm::mojom::UkmRecorderInterface:
  void AddEntry(mojom::UkmEntryPtr entry) override;

  UkmRecorder* ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(UkmInterface);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_INTERFACE_H_
