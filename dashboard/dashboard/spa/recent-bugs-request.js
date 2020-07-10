/* Copyright 2019 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {RequestBase} from './request-base.js';

export class RecentBugsRequest extends RequestBase {
  constructor() {
    super({});
    this.method_ = 'POST';
  }

  get url_() {
    return RecentBugsRequest.URL;
  }

  get description_() {
    return `loading recent bugs`;
  }

  postProcess_(json) {
    return json.bugs;
  }
}
RecentBugsRequest.URL = '/api/bugs/recent';
