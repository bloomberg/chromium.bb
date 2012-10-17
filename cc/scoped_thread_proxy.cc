// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCScopedThreadProxy.h"

namespace cc {

CCScopedThreadProxy::CCScopedThreadProxy(CCThread* targetThread)
    : m_targetThread(targetThread)
    , m_shutdown(false)
{
}

CCScopedThreadProxy::~CCScopedThreadProxy()
{
}

}
