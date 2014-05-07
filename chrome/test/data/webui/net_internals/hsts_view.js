// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/*
 * A valid hash that can be set for a domain.
 * @type {string}
 */
var VALID_HASH = 'sha1/Guzek9lMwR3KeIS8wwS9gBvVtIg=';

/*
 * An invalid hash that can't be set for a domain.
 * @type {string}
 */
var INVALID_HASH = 'invalid';

/*
 * Possible results of an HSTS query.
 * @enum {number}
 */
var QueryResultType = {
  SUCCESS: 0,
  NOT_FOUND: 1,
  ERROR: 2
};

/**
 * A Task that waits for the results of an HSTS query.  Once the results are
 * received, checks them before completing.  Does not initiate the query.
 * @param {string} domain The domain expected in the returned results.
 * @param {bool} stsSubdomains Whether or not the stsSubdomains flag is expected
 *     to be set in the returned results.  Ignored on error and not found
 *     results.
 * @param {bool} pkpSubdomains Whether or not the pkpSubdomains flag is expected
 *     to be set in the returned results.  Ignored on error and not found
 *     results.
 * @param {number} stsObserved The time the STS policy was observed.
 * @param {number} pkpObserved The time the PKP policy was observed.
 * @param {string} publicKeyHashes Expected public key hashes.  Ignored on error
 *     error and not found results.
 * @param {QueryResultType} queryResultType The expected result type of the
 *     results of the query.
 * @extends {NetInternalsTest.Task}
 */
function CheckQueryResultTask(domain, stsSubdomains, pkpSubdomains,
                              stsObserved, pkpObserved, publicKeyHashes,
                              queryResultType) {
  this.domain_ = domain;
  this.stsSubdomains_ = stsSubdomains;
  this.pkpSubdomains_ = pkpSubdomains;
  this.stsObserved_ = stsObserved;
  this.pkpObserved_ = pkpObserved;
  this.publicKeyHashes_ = publicKeyHashes;
  this.queryResultType_ = queryResultType;
  NetInternalsTest.Task.call(this);
}

CheckQueryResultTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Starts watching for the query results.
   */
  start: function() {
    g_browser.addHSTSObserver(this);
  },

  /**
   * Callback from the BrowserBridge.  Validates |result| and completes the
   * task.
   * @param {object} result Results from the query.
   */
  onHSTSQueryResult: function(result) {
    // Ignore results after |this| is finished.
    if (!this.isDone()) {
      expectEquals(this.domain_, $(HSTSView.QUERY_INPUT_ID).value);

      // Each case has its own validation function because of the design of the
      // test reporting infrastructure.
      if (result.error != undefined) {
        this.checkError_(result);
      } else if (!result.result) {
        this.checkNotFound_(result);
      } else {
        this.checkSuccess_(result);
      }
      this.running_ = false;

      // Start the next task asynchronously, so it can add another HSTS observer
      // without getting the current result.
      window.setTimeout(this.onTaskDone.bind(this), 1);
    }
  },

  /**
   * On errors, checks the result.
   * @param {object} result Results from the query.
   */
  checkError_: function(result) {
    expectEquals(QueryResultType.ERROR, this.queryResultType_);
    expectEquals(result.error, $(HSTSView.QUERY_OUTPUT_DIV_ID).innerText);
  },

  /**
   * Checks the result when the entry was not found.
   * @param {object} result Results from the query.
   */
  checkNotFound_: function(result) {
    expectEquals(QueryResultType.NOT_FOUND, this.queryResultType_);
    expectEquals('Not found', $(HSTSView.QUERY_OUTPUT_DIV_ID).innerText);
  },

  /**
   * Checks successful results.
   * @param {object} result Results from the query.
   */
  checkSuccess_: function(result) {
    expectEquals(QueryResultType.SUCCESS, this.queryResultType_);
    expectEquals(this.stsSubdomains_, result.dynamic_sts_include_subdomains);
    expectEquals(this.pkpSubdomains_, result.dynamic_pkp_include_subdomains);
    expectLE(this.stsObserved_, result.dynamic_sts_observed);
    expectLE(this.pkpObserved_, result.dynamic_pkp_observed);

    // |public_key_hashes| is an old synonym for what is now
    // |preloaded_spki_hashes|, which in turn is a legacy synonym for
    // |static_spki_hashes|. Look for all three, and also for
    // |dynamic_spki_hashes|.
    if (typeof result.public_key_hashes === 'undefined')
      result.public_key_hashes = '';
    if (typeof result.preloaded_spki_hashes === 'undefined')
      result.preloaded_spki_hashes = '';
    if (typeof result.static_spki_hashes === 'undefined')
      result.static_spki_hashes = '';
    if (typeof result.dynamic_spki_hashes === 'undefined')
      result.dynamic_spki_hashes = '';

    var hashes = [];
    if (result.public_key_hashes)
      hashes.push(result.public_key_hashes);
    if (result.preloaded_spki_hashes)
      hashes.push(result.preloaded_spki_hashes);
    if (result.static_spki_hashes)
      hashes.push(result.static_spki_hashes);
    if (result.dynamic_spki_hashes)
      hashes.push(result.dynamic_spki_hashes);

    expectEquals(this.publicKeyHashes_, hashes.join(','));

    // Verify that the domain appears somewhere in the displayed text.
    outputText = $(HSTSView.QUERY_OUTPUT_DIV_ID).innerText;
    expectLE(0, outputText.search(this.domain_));
  }
};

/**
 * A Task to try and add an HSTS domain via the HTML form.  The task will wait
 * until the results from the automatically sent query have been received, and
 * then checks them against the expected values.
 * @param {string} domain The domain to send and expected to be returned.
 * @param {bool} stsSubdomains Whether the HSTS subdomain checkbox should be
 *     selected. Also the corresponding expected return value, in the success
 *     case.
 * @param {bool} pkpSubdomains Whether the pinning subdomain checkbox should be
 *     selected. Also the corresponding expected return value, in the success
 *     case.
 * @param {number} stsObserved The time the STS policy was observed.
 * @param {number} pkpObserved The time the PKP policy was observed.
 * @param {string} publicKeyHashes Public key hash to send.  Also the
 *     corresponding expected return value, on success.  When this is the string
 *     INVALID_HASH, an empty string is expected to be received instead.
 * @param {QueryResultType} queryResultType Expected result type.
 * @extends {CheckQueryResultTask}
 */
function AddTask(domain, stsSubdomains, pkpSubdomains, publicKeyHashes,
                 stsObserved, pkpObserved, queryResultType) {
  this.requestedPublicKeyHashes_ = publicKeyHashes;
  if (publicKeyHashes == INVALID_HASH)
    publicKeyHashes = '';
  CheckQueryResultTask.call(this, domain, stsSubdomains, pkpSubdomains,
                            stsObserved, pkpObserved, publicKeyHashes,
                            queryResultType);
}

AddTask.prototype = {
  __proto__: CheckQueryResultTask.prototype,

  /**
   * Fills out the add form, simulates a click to submit it, and starts
   * listening for the results of the query that is automatically submitted.
   */
  start: function() {
    $(HSTSView.ADD_INPUT_ID).value = this.domain_;
    $(HSTSView.ADD_STS_CHECK_ID).checked = this.stsSubdomains_;
    $(HSTSView.ADD_PKP_CHECK_ID).checked = this.pkpSubdomains_;
    $(HSTSView.ADD_PINS_ID).value = this.requestedPublicKeyHashes_;
    $(HSTSView.ADD_SUBMIT_ID).click();
    CheckQueryResultTask.prototype.start.call(this);
  }
};

/**
 * A Task to query a domain and wait for the results.  Parameters mirror those
 * of CheckQueryResultTask, except |domain| is also the name of the domain to
 * query.
 * @extends {CheckQueryResultTask}
 */
function QueryTask(domain, stsSubdomains, pkpSubdomains, stsObserved,
                   pkpObserved, publicKeyHashes, queryResultType) {
  CheckQueryResultTask.call(this, domain, stsSubdomains, pkpSubdomains,
                            stsObserved, pkpObserved, publicKeyHashes,
                            queryResultType);
}

QueryTask.prototype = {
  __proto__: CheckQueryResultTask.prototype,

  /**
   * Fills out the query form, simulates a click to submit it, and starts
   * listening for the results.
   */
  start: function() {
    CheckQueryResultTask.prototype.start.call(this);
    $(HSTSView.QUERY_INPUT_ID).value = this.domain_;
    $(HSTSView.QUERY_SUBMIT_ID).click();
  }
};

/**
 * Task that deletes a single domain, then queries the deleted domain to make
 * sure it's gone.
 * @param {string} domain The domain to delete.
 * @param {QueryResultType} queryResultType The result of the query.  Can be
 *     QueryResultType.ERROR or QueryResultType.NOT_FOUND.
 * @extends {QueryTask}
 */
function DeleteTask(domain, queryResultType) {
  expectNotEquals(queryResultType, QueryResultType.SUCCESS);
  this.domain_ = domain;
  QueryTask.call(this, domain, false, false, '', 0, 0, queryResultType);
}

DeleteTask.prototype = {
  __proto__: QueryTask.prototype,

  /**
   * Fills out the delete form and simulates a click to submit it.  Then sends
   * a query.
   */
  start: function() {
    $(HSTSView.DELETE_INPUT_ID).value = this.domain_;
    $(HSTSView.DELETE_SUBMIT_ID).click();
    QueryTask.prototype.start.call(this);
  }
};

/**
 * Checks that querying a domain that was never added fails.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewQueryNotFound', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  var now = new Date().getTime() / 1000.0;
  taskQueue.addTask(new QueryTask('somewhere.com', false, false, now, now, '',
                                  QueryResultType.NOT_FOUND));
  taskQueue.run();
});

/**
 * Checks that querying a domain with an invalid name returns an error.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewQueryError', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  var now = new Date().getTime() / 1000.0;
  taskQueue.addTask(new QueryTask('\u3024', false, false, now, now, '',
                                  QueryResultType.ERROR));
  taskQueue.run();
});

/**
 * Deletes a domain that was never added.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewDeleteNotFound', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

/**
 * Deletes a domain that returns an error on lookup.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewDeleteError', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(new DeleteTask('\u3024', QueryResultType.ERROR));
  taskQueue.run();
});

/**
 * Adds a domain and then deletes it.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewAddDelete', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  var now = new Date().getTime() / 1000.0;
  taskQueue.addTask(new AddTask('somewhere.com', false, false, VALID_HASH,
                                now, now, QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

/**
 * Tries to add a domain with an invalid name.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewAddFail', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  var now = new Date().getTime() / 1000.0;
  taskQueue.addTask(new AddTask('0123456789012345678901234567890' +
                                '012345678901234567890123456789012345',
                                false, false, '', now, now,
                                QueryResultType.NOT_FOUND));
  taskQueue.run();
});

/**
 * Tries to add a domain with a name that errors out on lookup due to having
 * non-ASCII characters in it.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewAddError', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  var now = new Date().getTime() / 1000.0;
  taskQueue.addTask(new AddTask('\u3024', false, false, '', now, now,
                                QueryResultType.ERROR));
  taskQueue.run();
});

/**
 * Adds a domain with an invalid hash.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewAddInvalidHash', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  var now = new Date().getTime() / 1000.0;
  taskQueue.addTask(new AddTask('somewhere.com', true, true, INVALID_HASH,
                                now, now, QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

/**
 * Adds the same domain twice in a row, modifying some values the second time.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewAddOverwrite', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  var now = new Date().getTime() / 1000.0;
  taskQueue.addTask(new AddTask('somewhere.com', true, true, VALID_HASH,
                                now, now, QueryResultType.SUCCESS));
  taskQueue.addTask(new AddTask('somewhere.com', false, false, '',
                                now, now, QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

/**
 * Adds two different domains and then deletes them.
 */
TEST_F('NetInternalsTest', 'netInternalsHSTSViewAddTwice', function() {
  NetInternalsTest.switchToView('hsts');
  taskQueue = new NetInternalsTest.TaskQueue(true);
  var now = new Date().getTime() / 1000.0;
  taskQueue.addTask(new AddTask('somewhere.com', false, false, VALID_HASH,
                                now, now, QueryResultType.SUCCESS));
  taskQueue.addTask(new QueryTask('somewhereelse.com', false, false, now, now,
                                  '', QueryResultType.NOT_FOUND));
  taskQueue.addTask(new AddTask('somewhereelse.com', true, true, '',
                                now, now, QueryResultType.SUCCESS));
  taskQueue.addTask(new QueryTask('somewhere.com', false, false, now, now,
                                  VALID_HASH, QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.addTask(new QueryTask('somewhereelse.com', true, true, now, now, '',
                                  QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhereelse.com',
                                   QueryResultType.NOT_FOUND));
  taskQueue.run(true);
});

})();  // Anonymous namespace
