// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_TRANSLATOR_H_
#define CHROMEOS_NETWORK_ONC_ONC_TRANSLATOR_H_

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace onc {

struct OncValueSignature;

// Translates a hierarchical ONC dictionary |onc_object| to a flat Shill
// dictionary. The |signature| declares the type of |onc_object| and must point
// to one of the signature objects in "onc_signature.h". The resulting Shill
// dictionary is returned.
//
// This function is used to translate network settings from ONC to Shill's
// format before sending them to Shill.
CHROMEOS_EXPORT
scoped_ptr<base::DictionaryValue> TranslateONCObjectToShill(
    const OncValueSignature* signature,
    const base::DictionaryValue& onc_object);

// Translates a |shill_dictionary| to an ONC object according to the given
// |onc_signature|. |onc_signature| must point to a signature object in
// "onc_signature.h". The resulting ONC object is returned.
//
// This function is used to translate network settings coming from Shill to ONC
// before sending them to the UI. The result doesn't have to be valid ONC, but
// only a subset of it and includes only the values that are actually required
// by the UI.
CHROMEOS_EXPORT
scoped_ptr<base::DictionaryValue> TranslateShillServiceToONCPart(
    const base::DictionaryValue& shill_dictionary,
    const OncValueSignature* onc_signature);

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_TRANSLATOR_H_
