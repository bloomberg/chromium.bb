// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "PluginTest.h"

using namespace std;

class LeakWindowScriptableObject : public PluginTest {
public:
    LeakWindowScriptableObject(NPP npp, const string& identifier)
        : PluginTest(npp, identifier)
    {
    }

private:
 virtual NPError NPP_New(NPMIMEType pluginType,
                         uint16_t mode,
                         int16_t argc,
                         char* argn[],
                         char* argv[],
                         NPSavedData* saved) OVERRIDE {
        // Get a new reference to the window script object.
        NPObject* window;
        if (NPN_GetValue(NPNVWindowNPObject, &window) != NPERR_NO_ERROR) {
            log("Fail: Cannot fetch window script object");
            return NPERR_NO_ERROR;
        }

        // Get another reference to the same object via window.self.
        NPIdentifier self_name = NPN_GetStringIdentifier("self");
        NPVariant window_self_variant;
        if (!NPN_GetProperty(window, self_name, &window_self_variant)) {
            log("Fail: Cannot query window.self");
            return NPERR_NO_ERROR;
        }
        if (!NPVARIANT_IS_OBJECT(window_self_variant)) {
            log("Fail: window.self is not an object");
            return NPERR_NO_ERROR;
        }

        // Leak both references to the window script object.
        return NPERR_NO_ERROR;
    }
};

static PluginTest::Register<LeakWindowScriptableObject> leakWindowScriptableObject("leak-window-scriptable-object");
