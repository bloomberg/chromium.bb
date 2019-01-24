// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_

#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"

#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPage : public md_downloads::mojom::Page {
 public:
  MockPage();
  ~MockPage() override;

  md_downloads::mojom::PagePtr BindAndGetPtr();

  MOCK_METHOD1(RemoveItem, void(int));
  MOCK_METHOD2(UpdateItem, void(int, md_downloads::mojom::DataPtr));
  MOCK_METHOD2(InsertItems,
               void(int, std::vector<md_downloads::mojom::DataPtr>));
  MOCK_METHOD0(ClearAll, void());

  mojo::Binding<md_downloads::mojom::Page> binding_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_
