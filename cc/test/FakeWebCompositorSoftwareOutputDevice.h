// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeWebCompositorSoftwareOutputDevice_h
#define FakeWebCompositorSoftwareOutputDevice_h

#include "SkDevice.h"
#include <public/WebCompositorSoftwareOutputDevice.h>
#include <public/WebImage.h>
#include <public/WebSize.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class FakeWebCompositorSoftwareOutputDevice : public WebCompositorSoftwareOutputDevice {
public:
    virtual WebImage* lock(bool forWrite) OVERRIDE
    {
        ASSERT(m_device);
        m_image = m_device->accessBitmap(forWrite);
        return &m_image;
    }
    virtual void unlock() OVERRIDE
    {
        m_image.reset();
    }

    virtual void didChangeViewportSize(WebSize size) OVERRIDE
    {
        if (m_device.get() && size.width == m_device->width() && size.height == m_device->height())
            return;

        m_device = adoptPtr(new SkDevice(SkBitmap::kARGB_8888_Config, size.width, size.height, true));
    }

private:
    OwnPtr<SkDevice> m_device;
    WebImage m_image;
};

} // namespace WebKit

#endif // FakeWebCompositorSoftwareOutputDevice_h
