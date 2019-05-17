/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {assert} from 'chai';
window.assert = assert;
import testUtils from './cache-request-base.js';
import TimeseriesCacheRequest from './timeseries-cache-request.js';
import {
  getColumnsByLevelOfDetail,
  LEVEL_OF_DETAIL,
} from './timeseries-request.js';
import {
  normalize,
} from './utils.js';

suite('TimeseriesCacheRequest', function() {
  class MockFetchEvent {
    constructor(parameters) {
      parameters.columns = [...getColumnsByLevelOfDetail(
          parameters.levelOfDetail, parameters.statistic || 'avg')].join(',');
      parameters.test_suite = parameters.testSuite;
      parameters.min_revision = parameters.minRevision;
      parameters.max_revision = parameters.maxRevision;
      parameters = new Map(Object.entries(parameters));
      this.request = {
        url: 'http://example.com/path',
        clone() { return this; },
        async formData() { return parameters; },
      };
    }

    waitUntil() {
    }
  }

  function mockApiResponse({
    levelOfDetail, columns, minRevision = 1, maxRevision = 5}) {
    if (!columns) columns = getColumnsByLevelOfDetail(levelOfDetail, 'avg');
    const data = [];
    minRevision = parseInt(minRevision);
    for (let i = minRevision; i <= maxRevision; ++i) {
      const dataPoint = [];
      for (const _ of columns) {
        dataPoint.push(i);
      }
      data.push(dataPoint);
    }
    return {
      improvement_direction: 1,
      units: 'sizeInBytes_smallerIsBetter',
      data,
    };
  }

  function mockFetch(response) {
    window.fetch = async() => testUtils.jsonResponse(response);
  }

  async function deleteDatabase(parameters) {
    const databaseName = TimeseriesCacheRequest.databaseName(parameters);
    await testUtils.flushWriterForTest();
    await testUtils.deleteDatabaseForTest(databaseName);
  }

  let originalFetch;
  setup(() => {
    originalFetch = window.fetch;
  });
  teardown(() => {
    window.fetch = originalFetch;
  });

  test('yields only network', async() => {
    const parameters = {
      testSuite: 'yields only network',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
    };
    await deleteDatabase(parameters);

    const response = mockApiResponse(parameters);
    mockFetch(response);

    const request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    const results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    assert.deepEqual(results[0], response);
  });

  test('handles same ranges', async() => {
    const parameters = {
      testSuite: 'handles same ranges',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
    };
    await deleteDatabase(parameters);

    let response = mockApiResponse(parameters);
    mockFetch(response);

    let request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    let results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }
    assert.lengthOf(results, 1);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    assert.deepEqual(results[0], response);

    await testUtils.flushWriterForTest();
    response = mockApiResponse(parameters);
    mockFetch(response);

    request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 2);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    delete results[1].columns;
    assert.deepInclude(results, response);
  });

  test('handles separate ranges', async() => {
    const parameters = {
      testSuite: 'handles separate ranges',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
      minRevision: 1,
      maxRevision: 3,
    };
    await deleteDatabase(parameters);

    let response = mockApiResponse(parameters);
    mockFetch(response);

    let request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    let results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }
    assert.lengthOf(results, 1);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    assert.deepEqual(results[0], response);

    await testUtils.flushWriterForTest();
    parameters.minRevision = 4;
    parameters.maxRevision = 7;
    response = mockApiResponse(parameters);

    mockFetch(response);

    request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    // Only results from the network should be returned since the previous range
    // was [1, 3] and the current range is [4, 7].
    assert.lengthOf(results, 1);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    assert.deepEqual(results[0], response);
  });

  test('handles overlapping ranges', async() => {
    const parameters = {
      testSuite: 'handles overlapping ranges',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
      minRevision: 1,
      maxRevision: 3,
    };
    await deleteDatabase(parameters);

    let response = mockApiResponse(parameters);
    mockFetch(response);

    let request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    let results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    assert.deepEqual(results[0], response);

    parameters.minRevision = 2;
    parameters.maxRevision = 4;
    response = mockApiResponse(parameters);
    mockFetch(response);

    await testUtils.flushWriterForTest();
    request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    // Results from the cache and network should be returned since the previous
    // range [1, 3] intersects with the current range [2, 4].
    parameters.minRevision = 2;
    parameters.maxRevision = 3;
    const cacheResponse = mockApiResponse(parameters);
    cacheResponse.data = cacheResponse.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));

    assert.lengthOf(results, 2);
    delete results[0].columns;
    delete results[1].columns;
    assert.deepInclude(results, cacheResponse);
    assert.deepInclude(results, response);
  });

  test('handles multiple separate ranges', async() => {
    // Start a request with the first data point
    const parameters = {
      testSuite: 'handles multiple separate ranges',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
      minRevision: 1,
      maxRevision: 1,
    };
    await deleteDatabase(parameters);

    let response = mockApiResponse(parameters);
    mockFetch(response);

    let request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    let results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    assert.deepEqual(results[0], response);

    parameters.minRevision = 3;
    parameters.maxRevision = 3;
    response = mockApiResponse(parameters);
    mockFetch(response);

    await testUtils.flushWriterForTest();
    request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    assert.deepEqual(results[0], response);

    parameters.minRevision = 2;
    parameters.maxRevision = 2;
    response = mockApiResponse(parameters);
    mockFetch(response);

    await testUtils.flushWriterForTest();
    request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 1);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    assert.deepEqual(results[0], response);

    parameters.minRevision = 1;
    parameters.maxRevision = 3;
    response = mockApiResponse(parameters);
    mockFetch(response);

    await testUtils.flushWriterForTest();
    request = new TimeseriesCacheRequest(new MockFetchEvent(parameters));
    results = [];
    for await (const result of request.generateResults()) {
      results.push(result);
    }

    assert.lengthOf(results, 2);
    response.data = response.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    delete results[0].columns;
    delete results[1].columns;
    assert.deepInclude(results, response);
  });

  test('in progress different measurement', async() => {
    const parametersA = {
      testSuite: 'in progress different measurement',
      measurement: 'AAA',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
    };
    const parametersB = {
      ...parametersA,
      measurement: 'BBB',
    };
    const requestA = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersA));
    const requestB = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersB));

    let fetches = 0;
    window.fetch = async(url, options) => {
      ++fetches;
      return testUtils.jsonResponse(mockApiResponse({
        levelOfDetail: LEVEL_OF_DETAIL.XY,
        minRevision: options.body.get('min_revision') || undefined,
        maxRevision: options.body.get('max_revision') || undefined,
      }));
    };

    // Start A before B
    const resultsA = [];
    const doneA = (async() => {
      for await (const result of requestA.generateResults()) {
        resultsA.push(result);
      }
    })();
    const resultsB = [];
    const doneB = (async() => {
      for await (const result of requestB.generateResults()) {
        resultsB.push(result);
      }
    })();
    await Promise.all([doneA, doneB]);

    assert.strictEqual(2, fetches);

    const responseA = mockApiResponse(parametersA);
    responseA.data = responseA.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    for (const results of resultsA) delete results.columns;
    assert.deepInclude(resultsA, responseA);

    const responseB = mockApiResponse(parametersB);
    responseB.data = responseB.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    for (const results of resultsB) delete results.columns;
    assert.deepInclude(resultsB, responseB);
  });

  test('in progress containing', async() => {
    const parametersA = {
      testSuite: 'in progress containing',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
    };
    const parametersB = {
      ...parametersA,
      minRevision: 2,
      maxRevision: 4,
    };
    const requestA = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersA));
    const requestB = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersB));

    let fetches = 0;
    window.fetch = async(url, options) => {
      ++fetches;
      return testUtils.jsonResponse(mockApiResponse({
        levelOfDetail: LEVEL_OF_DETAIL.XY,
        minRevision: options.body.get('min_revision') || undefined,
        maxRevision: options.body.get('max_revision') || undefined,
      }));
    };

    // Start A before B
    const resultsA = [];
    const doneA = (async() => {
      for await (const result of requestA.generateResults()) {
        resultsA.push(result);
      }
    })();
    const resultsB = [];
    const doneB = (async() => {
      for await (const result of requestB.generateResults()) {
        resultsB.push(result);
      }
    })();
    await Promise.all([doneA, doneB]);

    assert.strictEqual(1, fetches);

    const responseA = mockApiResponse(parametersA);
    responseA.data = responseA.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    for (const results of resultsA) delete results.columns;
    assert.deepInclude(resultsA, responseA);

    const responseB = mockApiResponse(parametersB);
    responseB.data = responseB.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    for (const results of resultsB) delete results.columns;
    assert.deepInclude(resultsB, responseB);
  });

  test('in progress contained', async() => {
    const parametersA = {
      testSuite: 'in progress contained',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
    };
    const parametersB = {
      ...parametersA,
      minRevision: 2,
      maxRevision: 4,
    };
    const requestA = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersA));
    const requestB = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersB));

    let fetches = 0;
    window.fetch = async(url, options) => {
      ++fetches;
      return testUtils.jsonResponse(mockApiResponse({
        levelOfDetail: LEVEL_OF_DETAIL.XY,
        minRevision: options.body.get('min_revision') || undefined,
        maxRevision: options.body.get('max_revision') || undefined,
      }));
    };

    // Start B before A
    const resultsB = [];
    const doneB = (async() => {
      for await (const result of requestB.generateResults()) {
        resultsB.push(result);
      }
    })();
    const resultsA = [];
    const doneA = (async() => {
      for await (const result of requestA.generateResults()) {
        resultsA.push(result);
      }
    })();
    await Promise.all([doneA, doneB]);

    assert.strictEqual(1, fetches);

    const responseA = mockApiResponse(parametersA);
    responseA.data = responseA.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    for (const results of resultsA) delete results.columns;
    assert.deepInclude(resultsA, responseA);

    const responseB = mockApiResponse(parametersB);
    responseB.data = responseB.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    for (const results of resultsB) delete results.columns;
    assert.deepInclude(resultsB, responseB);
  });

  test('in progress containing xy, annotations', async() => {
    let fetches = 0;
    window.fetch = async(url, options) => {
      ++fetches;
      return testUtils.jsonResponse(mockApiResponse({
        columns: options.body.get('columns').split(','),
        minRevision: options.body.get('min_revision') || undefined,
        maxRevision: options.body.get('max_revision') || undefined,
      }));
    };

    const parametersA = {
      testSuite: 'in progress containing xy, annotations',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
    };
    const parametersB = {
      ...parametersA,
      minRevision: 2,
      maxRevision: 4,
      levelOfDetail: LEVEL_OF_DETAIL.ANNOTATIONS,
    };
    await deleteDatabase(parametersA);
    await deleteDatabase(parametersB);
    const requestA = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersA));
    const requestB = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersB));

    // Start A before B
    const resultsA = [];
    const doneA = (async() => {
      for await (const result of requestA.generateResults()) {
        resultsA.push(result);
      }
    })();
    const resultsB = [];
    const doneB = (async() => {
      for await (const result of requestB.generateResults()) {
        resultsB.push(result);
      }
    })();
    await Promise.all([doneA, doneB]);

    assert.strictEqual(2, fetches);

    const responseA = mockApiResponse(parametersA);
    responseA.data = responseA.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    for (const results of resultsA) delete results.columns;
    assert.deepInclude(resultsA, responseA);

    const responseB = mockApiResponse(parametersB);
    responseB.data = responseB.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(
            LEVEL_OF_DETAIL.ANNOTATIONS, 'avg')], d));
    for (const results of resultsB) delete results.columns;
    assert.deepInclude(resultsB, responseB);
  });

  test('in progress contained xy, annotations', async() => {
    const parametersA = {
      testSuite: 'in progress contained xy, annotations',
      measurement: 'measurement',
      bot: 'bot',
      statistic: 'avg',
      levelOfDetail: LEVEL_OF_DETAIL.XY,
    };
    const parametersB = {
      ...parametersA,
      minRevision: 2,
      maxRevision: 4,
      levelOfDetail: LEVEL_OF_DETAIL.ANNOTATIONS,
    };
    await deleteDatabase(parametersA);
    await deleteDatabase(parametersB);
    const requestA = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersA));
    const requestB = new TimeseriesCacheRequest(new MockFetchEvent(
        parametersB));

    let fetches = 0;
    window.fetch = async(url, options) => {
      ++fetches;
      return testUtils.jsonResponse(mockApiResponse({
        columns: options.body.get('columns').split(','),
        minRevision: options.body.get('min_revision') || undefined,
        maxRevision: options.body.get('max_revision') || undefined,
      }));
    };

    // Start B before A
    const resultsB = [];
    const doneB = (async() => {
      for await (const result of requestB.generateResults()) {
        resultsB.push(result);
      }
    })();
    const resultsA = [];
    const doneA = (async() => {
      for await (const result of requestA.generateResults()) {
        resultsA.push(result);
      }
    })();
    await Promise.all([doneA, doneB]);

    assert.strictEqual(2, fetches);

    const responseA = mockApiResponse(parametersA);
    responseA.data = responseA.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(LEVEL_OF_DETAIL.XY, 'avg')], d));
    for (const results of resultsA) delete results.columns;
    assert.deepInclude(resultsA, responseA);

    const responseB = mockApiResponse(parametersB);
    responseB.data = responseB.data.map(d => normalize(
        [...getColumnsByLevelOfDetail(
            LEVEL_OF_DETAIL.ANNOTATIONS, 'avg')], d));
    for (const results of resultsB) delete results.columns;
    assert.deepInclude(resultsB, responseB);
  });
});
