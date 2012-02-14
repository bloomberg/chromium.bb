// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_hdc.h"

#include "base/logging.h"

namespace base {
namespace win {

ScopedDC::ScopedDC(HDC hdc)
    : hdc_(hdc),
      bitmap_(0),
      font_(0),
      brush_(0),
      pen_(0),
      region_(0) {
}

ScopedDC::~ScopedDC() {}

void ScopedDC::SelectBitmap(HBITMAP bitmap) {
  Select(bitmap, &bitmap_);
}

void ScopedDC::SelectFont(HFONT font) {
  Select(font, &font_);
}

void ScopedDC::SelectBrush(HBRUSH brush) {
  Select(brush, &brush_);
}

void ScopedDC::SelectPen(HPEN pen) {
  Select(pen, &pen_);
}

void ScopedDC::SelectRegion(HRGN region) {
  Select(region, &region_);
}

void ScopedDC::Close() {
  if (!hdc_)
    return;
  ResetObjects();
  DisposeDC(hdc_);
}

void ScopedDC::Reset(HDC hdc) {
  Close();
  hdc_ = hdc;
}

void ScopedDC::ResetObjects() {
  if (bitmap_) {
    SelectObject(hdc_, bitmap_);
    bitmap_ = 0;
  }
  if (font_) {
    SelectObject(hdc_, font_);
    font_ = 0;
  }
  if (brush_) {
    SelectObject(hdc_, brush_);
    brush_ = 0;
  }
  if (pen_) {
    SelectObject(hdc_, pen_);
    pen_ = 0;
  }
  if (region_) {
    SelectObject(hdc_, region_);
    region_ = 0;
  }
}

void ScopedDC::Select(HGDIOBJ object, HGDIOBJ* holder) {
  HGDIOBJ old = SelectObject(hdc_, object);
  DCHECK(old);
  // We only want to store the first |old| object.
  if (!*holder)
    *holder = old;
}

ScopedGetDC::ScopedGetDC(HWND hwnd) : ScopedDC(GetDC(hwnd)), hwnd_(hwnd) {
}

ScopedGetDC::~ScopedGetDC() {
  Close();
}

void ScopedGetDC::DisposeDC(HDC hdc) {
  ReleaseDC(hwnd_, hdc);
}

ScopedCreateDC::ScopedCreateDC() : ScopedDC(0) {
}

ScopedCreateDC::ScopedCreateDC(HDC hdc) : ScopedDC(hdc) {
}

ScopedCreateDC::~ScopedCreateDC() {
  Close();
}

void ScopedCreateDC::Set(HDC hdc) {
  Reset(hdc);
}

void ScopedCreateDC::DisposeDC(HDC hdc) {
  DeleteDC(hdc);
}

}  // namespace win
}  // namespace base
