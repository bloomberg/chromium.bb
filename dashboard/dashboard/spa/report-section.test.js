/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {CHAIN, ENSURE, UPDATE} from './simple-redux.js';
import {ReportControls} from './report-controls.js';
import {ReportNamesRequest} from './report-names-request.js';
import {ReportRequest} from './report-request.js';
import {ReportSection} from './report-section.js';
import {STORE} from './element-base.js';
import {afterRender} from './utils.js';
import {assert} from 'chai';

suite('report-section', function() {
  let originalFetch;
  setup(async() => {
    originalFetch = window.fetch;
    window.fetch = async(url, options) => {
      return {
        ok: true,
        async json() {
          if (url === ReportNamesRequest.URL) {
            return [{
              name: ReportControls.DEFAULT_NAME,
              id: 42,
              modified: new Date(),
            }];
          }
          if (url === ReportRequest.URL) {
            return {
              name: ReportControls.DEFAULT_NAME,
              owners: ['me@chromium.org'],
              url: 'http://example.com/',
              report: {
                rows: [
                  {
                    label: 'group:label',
                    units: 'ms_smallerIsBetter',
                    testSuites: ['suite'],
                    bots: ['master:bot'],
                    testCases: ['case'],
                    measurement: 'measurement',
                    data: {
                      [ReportControls.CHROMIUM_MILESTONES[
                          ReportControls.CURRENT_MILESTONE]]: {
                        descriptors: [
                          {
                            testSuite: 'suite',
                            measurement: 'measurement',
                            bot: 'master:bot',
                            testCase: 'case',
                          },
                        ],
                        statistics: [10, 0, 0, 100, 0, 0, 100],
                        revision: ReportControls.CHROMIUM_MILESTONES[
                            ReportControls.CURRENT_MILESTONE],
                      },
                      latest: {
                        descriptors: [
                          {
                            testSuite: 'suite',
                            measurement: 'measurement',
                            bot: 'master:bot',
                            testCase: 'case',
                          },
                        ],
                        statistics: [10, 0, 0, 100, 0, 0, 100],
                        revision: 999999,
                      },
                    },
                  },
                ],
                statistics: ['avg'],
              },
            };
          }
        },
      };
    };
  });
  teardown(() => {
    for (const child of document.body.children) {
      if (!child.matches('report-section')) continue;
      document.body.removeChild(child);
    }
    window.fetch = originalFetch;
  });

  test('loadReports', async function() {
    const section = document.createElement('report-section');
    section.statePath = 'test';
    await STORE.dispatch(CHAIN(
        ENSURE('test'),
        ENSURE('test.source'),
        UPDATE('test', ReportSection.buildState({}))));
    document.body.appendChild(section);
    await afterRender();
    assert.isTrue(section.tables[0].isPlaceholder);
    while (section.tables[0].isPlaceholder) {
      await afterRender();
    }
    assert.lengthOf(section.tables[0].rows, 1);
    assert.strictEqual('group', section.tables[0].rows[0].labelParts[0].label);
    assert.strictEqual('label', section.tables[0].rows[0].labelParts[1].label);
    assert.strictEqual(100, section.tables[0].rows[0].scalars[0].value);
    assert.strictEqual(100, section.tables[0].rows[0].scalars[1].value);
  });
});
