// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_MODAL_DIALOGS_MESSAGE_BOX_HANDLER_H_
#define CHROME_BROWSER_UI_APP_MODAL_DIALOGS_MESSAGE_BOX_HANDLER_H_
#pragma once

namespace content {
class JavaScriptDialogCreator;
}

// Returns a JavaScriptDialogCreator that creates real dialogs.
content::JavaScriptDialogCreator* GetJavaScriptDialogCreatorInstance();

#endif  // CHROME_BROWSER_UI_APP_MODAL_DIALOGS_MESSAGE_BOX_HANDLER_H_
