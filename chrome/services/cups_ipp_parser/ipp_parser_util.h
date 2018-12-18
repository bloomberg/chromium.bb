// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_IPP_PARSER_IPP_PARSER_UTIL_H_
#define CHROME_SERVICES_CUPS_IPP_PARSER_IPP_PARSER_UTIL_H_

#include <cups/cups.h>

#include "chrome/services/cups_ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "printing/backend/cups_ipp_util.h"

namespace chrome {

// Parses and converts |ipp| to the corresponding mojom type for marshalling.
mojom::IppMessagePtr ParseIppMessage(ipp_t* ipp);

// Synchronously reads/parses |ipp_slice| and returns the resulting ipp_t
// object.
printing::ScopedIppPtr ReadIppSlice(base::StringPiece ipp_slice);

}  // namespace chrome

#endif  // CHROME_SERVICES_CUPS_IPP_PARSER_IPP_PARSER_UTIL_H_
