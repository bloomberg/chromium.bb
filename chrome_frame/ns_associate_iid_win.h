// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_NS_ASSOCIATE_IID_WIN_H_
#define CHROME_FRAME_NS_ASSOCIATE_IID_WIN_H_

#define ASSOCIATE_IID(iid, class_name) class __declspec(uuid(iid)) class_name

// Associate IIDs with ns interfaces for convenience.
// This makes ScopedNsPtr more user friendly and avoids the nsIXyz::GetIID()
// calls which can't be used in templates (runtime vs compile time).
ASSOCIATE_IID("00000000-0000-0000-c000-000000000046", nsISupports);

#endif  // CHROME_FRAME_NS_ASSOCIATE_IID_WIN_H_
