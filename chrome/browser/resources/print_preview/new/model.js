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
 *   copiesInvalid: boolean,
 *   scalingInvalid: boolean,
 *   pagesInvalid: boolean,
 *   isPdfDocument: boolean,
 *   fitToPageScaling: string,
 *   documentNumPages: number,
 * }}
 */
print_preview_new.Model;
