// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTOTEST_SHELF_INTEGRATION_TEST_API_H_
#define ASH_AUTOTEST_SHELF_INTEGRATION_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/shelf_integration_test_api.mojom.h"
#include "base/macros.h"

namespace ash {

// Allows tests to access private state of the shelf.
class ASH_EXPORT ShelfIntegrationTestApi
    : public mojom::ShelfIntegrationTestApi {
 public:
  // Binds the mojom::ShelfIntegrationTestApiRequest interface request to this
  // object.
  static void BindRequest(mojom::ShelfIntegrationTestApiRequest request);

  ShelfIntegrationTestApi();
  ~ShelfIntegrationTestApi() override;

  // mojom::ShelfIntegrationTestApi:
  void GetAutoHideBehavior(int64_t display_id,
                           GetAutoHideBehaviorCallback callback) override;
  void SetAutoHideBehavior(int64_t display_id,
                           ShelfAutoHideBehavior behavior,
                           SetAutoHideBehaviorCallback callback) override;
  void GetAlignment(int64_t display_id, GetAlignmentCallback callback) override;
  void SetAlignment(int64_t display_id,
                    ShelfAlignment behavior,
                    SetAlignmentCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfIntegrationTestApi);
};

}  // namespace ash

#endif  // ASH_AUTOTEST_SHELF_INTEGRATION_TEST_API_H_
