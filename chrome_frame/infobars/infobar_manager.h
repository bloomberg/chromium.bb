// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_INFOBARS_INFOBAR_MANAGER_H_
#define CHROME_FRAME_INFOBARS_INFOBAR_MANAGER_H_

#include <windows.h>

class InfobarContent;

enum InfobarType {
  FIRST_INFOBAR_TYPE = 0,
  TOP_INFOBAR = 0,          // Infobar at the top.
  BOTTOM_INFOBAR = 1,       // Infobar at the bottom.
  END_OF_INFOBAR_TYPE = 2
};

// Creates and manages infobars at the top or bottom of an IE content window.
// Instances must only be retrieved and used within the UI thread of the IE
// content window.
class InfobarManager {
 public:
  // Returns an InfobarManager for the specified IE tab window. Caller does not
  // own the pointer (resources will be freed when the window is destroyed).
  //
  // The pointer may be invalidated by further processing of window events, and
  // as such should be immediately discarded after use.
  //
  // Returns NULL in case of failure.
  static InfobarManager* Get(HWND tab_window);

  virtual ~InfobarManager();

  // Shows the supplied content in an infobar of the specified type.
  // Normally, InfobarContent::InstallInFrame will be called with an interface
  // the content may use to interact with the Infobar facility.
  //
  // InfobarContent is deleted when the Infobar facility is finished with the
  // content (either through failure or when successfully hidden).
  virtual bool Show(InfobarContent* content, InfobarType type) = 0;

  // Hides the infobar of the specified type.
  virtual void Hide(InfobarType type) = 0;

  // Hides all infobars.
  virtual void HideAll() = 0;
};  // class InfobarManager

#endif  // CHROME_FRAME_INFOBARS_INFOBAR_MANAGER_H_
