/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import * as testUtils from './cache-request-base.js';
import {ReportCacheRequest} from './report-cache-request.js';
import {assert} from 'chai';
import {timeout} from './utils.js';

suite('ReportCacheRequest', function() {
  class MockFetchEvent {
    constructor(parameters) {
      this.request = {
        url: 'http://example.com/path',
        clone() {
          return this;
        },
        async formData() {
          return new Map([
            ['id', parameters.id],
            ['modified', parameters.modified.getTime()],
            ['revisions', parameters.revisions.join(',')],
          ]);
        }
      };
    }

    waitUntil() {
    }
  }

  function mockApiResponse(parameters) {
    const {
      id,
      name,
      revisions,
      editable = false,
      internal = false,
      owners = ['a@google.com', 'b@google.com', 'c@google.com'],
    } = parameters;

    const rows = [];
    for (const group of ['Pixel', 'Android Go']) {
      rows.push({
        ...generateRow(revisions, 'memory:a_size', 1),
        label: group + ':Memory',
        units: 'sizeInBytes_smallerIsBetter',
      });
      rows.push({
        ...generateRow(revisions, 'loading', 2),
        label: group + ':Loading',
        units: 'ms_smallerIsBetter',
      });
      rows.push({
        ...generateRow(revisions, 'startup', 3),
        label: group + ':Startup',
        units: 'ms_smallerIsBetter',
      });
      rows.push({
        ...generateRow(revisions, 'cpu:a', 4),
        label: group + ':CPU',
        units: 'ms_smallerIsBetter',
      });
      rows.push({
        ...generateRow(revisions, 'power', 5),
        label: group + ':Power',
        units: 'W_smallerIsBetter',
      });
    }

    return {
      editable,
      id,
      internal,
      name,
      owners,
      report: {
        rows,
        statistics: ['avg', 'std'],
      },
    };
  }

  function generateRow(revisions, measurement, seed) {
    const row = {
      testSuites: ['system_health.common_mobile'],
      bots: ['master:bot0', 'master:bot1', 'master:bot2'],
      testCases: [],
      data: {},
      measurement,
    };
    for (const revision of revisions) {
      row.data[revision] = {
        descriptors: [
          {
            testSuite: 'system_health.common_mobile',
            measurement,
            bot: 'master:bot0',
            testCase: 'search:portal:google',
          },
          {
            testSuite: 'system_health.common_mobile',
            measurement,
            bot: 'master:bot1',
            testCase: 'search:portal:google',
          },
        ],
        statistics: [10, 0, 0, seed * 1000, 0, 0, seed * 1000],
        revision,
      };
    }
    return row;
  }

  function mockFetch(response) {
    window.fetch = async() => {
      // This timeout is necessary for the tests that expect the database to
      // return before the network, as is typical in production. If the network
      // returns before the database, then generateResults() will not wait for
      // the database.
      await timeout(10);
      return testUtils.jsonResponse(response);
    };
  }

  let originalFetch;
  setup(() => {
    originalFetch = window.fetch;
  });
  teardown(() => {
    window.fetch = originalFetch;
  });

  async function deleteDatabase(parameters) {
    const databaseName = ReportCacheRequest.databaseName(parameters);
    await testUtils.flushWriterForTest();
    await testUtils.deleteDatabaseForTest(databaseName);
  }

  test('channelName', async function() {
    const parameters = {
      id: 10,
      name: 'channelName',
      modified: new Date(0),
      revisions: [1, 2, 3],
    };
    await deleteDatabase(parameters);

    const response = mockApiResponse(parameters);
    mockFetch(response);

    const request = new ReportCacheRequest(new MockFetchEvent(parameters));
    const channelName = await request.computeChannelName();
    assert.strictEqual(channelName, 'http://example.com/path?id=10&modified=0&revisions=1%2C2%2C3');
  });

  // Results from the network should be yielded when there is no existing data
  // in the cache.
  test('yields only network', async() => {
    const parameters = {
      id: 11,
      name: 'name',
      modified: new Date(0),
      revisions: [1, 2, 3],
    };
    await deleteDatabase(parameters);

    const response = mockApiResponse(parameters);
    mockFetch(response);

    const request = new ReportCacheRequest(new MockFetchEvent(parameters));
    const results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    assert.deepEqual(results[0], response);
  });

  // After receiving results from the network, the cache will start yielding
  // results in addition to the network.
  test('yields cache and network', async function() {
    const parameters = {
      id: 12,
      name: 'name',
      modified: new Date(0),
      revisions: [1, 2, 3],
    };
    await deleteDatabase(parameters);

    const response = mockApiResponse(parameters);
    mockFetch(response);

    let request = new ReportCacheRequest(new MockFetchEvent(parameters));
    let results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    assert.deepEqual(results[0], response);

    mockFetch(response);
    await testUtils.flushWriterForTest();
    request = new ReportCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 2);
    assert.deepInclude(results, response);
    assert.deepInclude(results, response);
  });

  // When template ids change, the cache should not yield non-relevant data.
  test('yields new data for different ids', async() => {
    const parameters = {
      a: {
        id: 13,
        name: 'name',
        modified: new Date(0),
        revisions: [1, 2, 3],
      },
      b: {
        id: 14,
        name: 'name',
        modified: new Date(0),
        revisions: [1, 2, 3],
      }
    };

    const databaseNameA = ReportCacheRequest.databaseName(parameters.a);
    const databaseNameB = ReportCacheRequest.databaseName(parameters.b);
    await testUtils.flushWriterForTest();
    await testUtils.deleteDatabaseForTest(databaseNameA);
    await testUtils.deleteDatabaseForTest(databaseNameB);

    const responseA = mockApiResponse(parameters.a);
    mockFetch(responseA);

    let request = new ReportCacheRequest(new MockFetchEvent(parameters.a));
    let results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    assert.deepEqual(results[0], responseA);

    await testUtils.flushWriterForTest();

    const responseB = mockApiResponse(parameters.b);
    mockFetch(responseB);

    // Request a report with a different id.
    mockFetch(responseB);
    request = new ReportCacheRequest(new MockFetchEvent(parameters.b));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    // Only results from the network should be returned since the revision range
    // is different.
    assert.lengthOf(results, 1);
    assert.deepEqual(results[0], responseB);
  });

  // If the "modified" query parameter is changed, the database should be wiped
  // clear of any previous data.
  test('yields new data for different modified dates', async() => {
    const parameters = {
      id: 15,
      name: 'name',
      modified: new Date(0),
      revisions: [1, 2, 3],
    };
    await deleteDatabase(parameters);

    let response = mockApiResponse(parameters);
    mockFetch(response);

    let request = new ReportCacheRequest(new MockFetchEvent(parameters));
    let results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    assert.deepEqual(results[0], response);

    await testUtils.flushWriterForTest();

    // Different "modified" dates should return unique results
    parameters.modified = new Date(1);
    response = mockApiResponse(parameters);
    mockFetch(response);

    request = new ReportCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    assert.deepEqual(results[0], response);

    await testUtils.flushWriterForTest();

    // Similar "modified" dates should return the same results.
    mockFetch(response);
    request = new ReportCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 2);
    assert.deepInclude(results, response);
    assert.deepInclude(results, response);
  });

  // When revisions change, the cache should not yield non-relevant data.
  test('yields new data for different revisions', async() => {
    const parameters = {
      id: 16,
      name: 'name',
      modified: new Date(0),
      revisions: [1, 2, 3],
    };
    await deleteDatabase(parameters);

    let response = mockApiResponse(parameters);

    mockFetch(response);
    let request = new ReportCacheRequest(new MockFetchEvent(parameters));
    let results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    assert.deepEqual(results[0], response);

    // Different revision ranges should return new data.
    parameters.revisions = [4, 5, 6];
    response = mockApiResponse(parameters);
    mockFetch(response);

    await testUtils.flushWriterForTest();
    request = new ReportCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    assert.deepEqual(results[0], response);
  });
});
