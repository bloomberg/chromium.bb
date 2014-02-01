// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "PluginTest.h"
#include <string.h>

extern NPNetscapeFuncs *browser;

class FormValue : public PluginTest {
public:
    FormValue(NPP npp, const std::string& identifier)
        : PluginTest(npp, identifier)
    {
    }
    virtual NPError NPP_GetValue(NPPVariable, void*) OVERRIDE;
};

NPError FormValue::NPP_GetValue(NPPVariable variable, void *value)
{
    if (variable == NPPVformValue) {
        static const char formValueText[] = "Plugin form value";
        *((void**)value) = browser->memalloc(sizeof(formValueText));
        if (!*((void**)value))
            return NPERR_OUT_OF_MEMORY_ERROR;
        strncpy(*((char**)value), formValueText, sizeof(formValueText));
        return NPERR_NO_ERROR;
    }
    return NPERR_GENERIC_ERROR;
}

static PluginTest::Register<FormValue> formValue("form-value");
