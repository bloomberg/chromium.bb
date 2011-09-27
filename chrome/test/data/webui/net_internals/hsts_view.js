// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
 * @param {bool} subdomains Whether or not the subdomains flag is expected to be
 *     set in the returned results.  Ignored on error and not found results.
 * @param {string} publicKeyHashes Expected public key hashes.  Ignored on error
 *     error and not found results.
 * @param {QueryResultType} queryResultType The expected result type of the
 *     results of the query.
 * @extends {netInternalsTest.Task}
 */
function CheckQueryResultTask(domain, subdomains, publicKeyHashes,
                              queryResultType) {
  this.domain_ = domain;
  this.subdomains_ = subdomains;
  this.publicKeyHashes_ = publicKeyHashes;
  this.queryResultType_ = queryResultType;
  this.running_ = false;
  netInternalsTest.Task.call(this);
}

CheckQueryResultTask.prototype = {
  __proto__: netInternalsTest.Task.prototype,

  /**
   * Starts watching for the query results.
   */
  start: function() {
    g_browser.addHSTSObserver(this);
    // This will be set to false to ignore results, once the task is complete.
    this.running_ = true;
  },

  /**
   * Callback from the BrowserBridge.  Validates |result| and completes the
   * task.
   * @param {object} result Results from the query.
   */
  onHSTSQueryResult: function(result) {
    if (this.running_) {
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
    expectEquals(this.subdomains_, result.subdomains);
    expectEquals(this.publicKeyHashes_, result.public_key_hashes);

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
 * @param {bool} subdomains Whether the subdomain checkbox should be selected.
 *     Also the corresponding expected return value, in the success case.
 * @param {string} publicKeyHashes Public key hash to send.  Also the
 *     corresponding expected return value, on success.  When this is the string
 *     INVALID_HASH, an empty string is expected to be received instead.
 * @param {QueryResultType} queryResultType Expected result type.
 * @extends {CheckQueryResultTask}
 */
function AddTask(domain, subdomains, publicKeyHashes, queryResultType) {
  this.requestedPublicKeyHashes_ = publicKeyHashes;
  if (publicKeyHashes == INVALID_HASH)
    publicKeyHashes = '';
  CheckQueryResultTask.call(this, domain, subdomains, publicKeyHashes,
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
    $(HSTSView.ADD_CHECK_ID).checked = this.subdomains_;
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
function QueryTask(domain, subdomains, publicKeyHashes, queryResultType) {
  CheckQueryResultTask.call(this, domain, subdomains, publicKeyHashes,
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
  QueryTask.call(this, domain, false, '', queryResultType);
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

netInternalsTest.test('netInternalsHSTSViewQueryNotFound', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new QueryTask('somewhere.com', false, '',
                                  QueryResultType.NOT_FOUND));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewQueryError', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new QueryTask('\u3024', false, '', QueryResultType.ERROR));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewDeleteNotFound', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewDeleteError', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new DeleteTask('\u3024', QueryResultType.ERROR));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewAddDelete', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddTask('somewhere.com', false, VALID_HASH,
                                QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewAddFail', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddTask('~', false, '', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewAddError', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddTask('\u3024', false, '', QueryResultType.ERROR));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewAddInvalidHash', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddTask('somewhere.com', true, INVALID_HASH,
                                QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewAddOverwrite', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddTask('somewhere.com', true, VALID_HASH,
                                QueryResultType.SUCCESS));
  taskQueue.addTask(new AddTask('somewhere.com', false, '',
                                QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.run();
});

netInternalsTest.test('netInternalsHSTSViewAddTwice', function() {
  netInternalsTest.switchToView('hsts');
  taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddTask('somewhere.com', false, VALID_HASH,
                                QueryResultType.SUCCESS));
  taskQueue.addTask(new QueryTask('somewhereelse.com', false, '',
                                  QueryResultType.NOT_FOUND));
  taskQueue.addTask(new AddTask('somewhereelse.com', true, '',
                                QueryResultType.SUCCESS));
  taskQueue.addTask(new QueryTask('somewhere.com', false, VALID_HASH,
                                  QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhere.com', QueryResultType.NOT_FOUND));
  taskQueue.addTask(new QueryTask('somewhereelse.com', true, '',
                                  QueryResultType.SUCCESS));
  taskQueue.addTask(new DeleteTask('somewhereelse.com',
                                   QueryResultType.NOT_FOUND));
  taskQueue.run(true);
});

})();  // Anonymous namespace
