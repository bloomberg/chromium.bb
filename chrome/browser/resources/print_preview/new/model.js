// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/**
 * @typedef {{
 *   previewLoading: boolean,
 *   previewFailed: boolean,
 *   cloudPrintError: string,
 *   privetExtensionError: string,
 *   invalidSettings: boolean,
 *   destinationId: string,
 *   copies: number,
 *   pageRange: !Array<number>,
 *   duplex: boolean,
 *   printTicketInvalid: boolean
 * }}
 */
print_preview_new.Model;
