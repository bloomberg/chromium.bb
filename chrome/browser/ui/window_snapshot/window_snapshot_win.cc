// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_snapshot/window_snapshot.h"

#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "gfx/codec/png_codec.h"
#include "gfx/gdi_util.h"
#include "gfx/rect.h"

namespace browser {

gfx::Rect GrabWindowSnapshot(gfx::NativeWindow window_handle,
                             std::vector<unsigned char>* png_representation) {
  // Create a memory DC that's compatible with the window.
  HDC window_hdc = GetWindowDC(window_handle);
  base::win::ScopedHDC mem_hdc(CreateCompatibleDC(window_hdc));

  // Create a DIB that's the same size as the window.
  RECT content_rect = {0, 0, 0, 0};
  ::GetWindowRect(window_handle, &content_rect);
  content_rect.right++;  // Match what PrintWindow wants.
  int width = content_rect.right - content_rect.left;
  int height = content_rect.bottom - content_rect.top;
  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(width, height, &hdr);
  unsigned char *bit_ptr = NULL;
  base::win::ScopedBitmap bitmap(
      CreateDIBSection(mem_hdc,
                       reinterpret_cast<BITMAPINFO*>(&hdr),
                       DIB_RGB_COLORS,
                       reinterpret_cast<void **>(&bit_ptr),
                       NULL, 0));

  SelectObject(mem_hdc, bitmap);
  // Clear the bitmap to white (so that rounded corners on windows
  // show up on a white background, and strangely-shaped windows
  // look reasonable). Not capturing an alpha mask saves a
  // bit of space.
  PatBlt(mem_hdc, 0, 0, width, height, WHITENESS);
  // Grab a copy of the window
  // First, see if PrintWindow is defined (it's not in Windows 2000).
  typedef BOOL (WINAPI *PrintWindowPointer)(HWND, HDC, UINT);
  PrintWindowPointer print_window =
      reinterpret_cast<PrintWindowPointer>(
          GetProcAddress(GetModuleHandle(L"User32.dll"), "PrintWindow"));

  // If PrintWindow is defined, use it.  It will work on partially
  // obscured windows, and works better for out of process sub-windows.
  // Otherwise grab the bits we can get with BitBlt; it's better
  // than nothing and will work fine in the average case (window is
  // completely on screen).
  if (print_window)
    (*print_window)(window_handle, mem_hdc, 0);
  else
    BitBlt(mem_hdc, 0, 0, width, height, window_hdc, 0, 0, SRCCOPY);

  // We now have a copy of the window contents in a DIB, so
  // encode it into a useful format for posting to the bug report
  // server.
  gfx::PNGCodec::Encode(bit_ptr, gfx::PNGCodec::FORMAT_BGRA,
                        width, height, width * 4, true,
                        png_representation);

  ReleaseDC(window_handle, window_hdc);

  return gfx::Rect(width, height);
}

}  // namespace browser
