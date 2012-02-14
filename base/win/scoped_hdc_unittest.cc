// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/basictypes.h"
#include "base/win/scoped_hdc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper class that allows testing ScopedDC<T>.
class TestScopedDC : public base::win::ScopedDC {
 public:
  explicit TestScopedDC(HDC hdc)
    : ScopedDC(hdc) {
  }

  virtual ~TestScopedDC() {
    Close();
  }

 private:
  virtual void DisposeDC(HDC hdc) OVERRIDE {
    // We leak the DC, so we can test its state. The test itself
    // will dispose of the dc later.
  }

  DISALLOW_COPY_AND_ASSIGN(TestScopedDC);
};

bool IsValidDC(HDC hdc) {
  // The theory here is that any (cheap) GDI operation should fail for
  // an invalid dc.
  return GetCurrentObject(hdc, OBJ_BITMAP) != NULL;
}

}  // namespace

TEST(BaseWinScopedDC, CreateDestroy) {
  HDC hdc1;
  {
    base::win::ScopedGetDC dc1(NULL);
    hdc1 = dc1.get();
    EXPECT_TRUE(IsValidDC(hdc1));
  }
  EXPECT_FALSE(IsValidDC(hdc1));

  HDC hdc2 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
  ASSERT_TRUE(IsValidDC(hdc2));
  {
    base::win::ScopedCreateDC dc2(hdc2);
    EXPECT_TRUE(IsValidDC(hdc2));
  }
  EXPECT_FALSE(IsValidDC(hdc2));
}

TEST(BaseWinScopedDC, SelectObjects) {
  HDC hdc = CreateCompatibleDC(NULL);
  ASSERT_TRUE(IsValidDC(hdc));
  HGDIOBJ bitmap = GetCurrentObject(hdc, OBJ_BITMAP);
  HGDIOBJ brush = GetCurrentObject(hdc, OBJ_BRUSH);
  HGDIOBJ pen = GetCurrentObject(hdc, OBJ_PEN);
  HGDIOBJ font = GetCurrentObject(hdc, OBJ_FONT);

  HBITMAP compat_bitmap = CreateCompatibleBitmap(hdc, 24, 24);
  ASSERT_TRUE(compat_bitmap != NULL);
  HBRUSH solid_brush = CreateSolidBrush(RGB(22, 33, 44));
  ASSERT_TRUE(solid_brush != NULL);

  {
    TestScopedDC dc2(hdc);
    dc2.SelectBitmap(compat_bitmap);
    dc2.SelectBrush(solid_brush);
    EXPECT_TRUE(bitmap != GetCurrentObject(hdc, OBJ_BITMAP));
    EXPECT_TRUE(brush != GetCurrentObject(hdc, OBJ_BRUSH));
  }

  EXPECT_TRUE(bitmap == GetCurrentObject(hdc, OBJ_BITMAP));
  EXPECT_TRUE(brush == GetCurrentObject(hdc, OBJ_BRUSH));
  EXPECT_TRUE(pen == GetCurrentObject(hdc, OBJ_PEN));
  EXPECT_TRUE(font == GetCurrentObject(hdc, OBJ_FONT));

  EXPECT_TRUE(DeleteDC(hdc));
  EXPECT_TRUE(DeleteObject(compat_bitmap));
  EXPECT_TRUE(DeleteObject(solid_brush));
}
