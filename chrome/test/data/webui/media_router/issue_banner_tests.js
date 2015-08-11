// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for issue-banner. */
cr.define('issue_banner', function() {
  function registerTests() {
    suite('IssueBanner', function() {
      /**
       * Issue Banner created before each test.
       * @type {IssueBanner}
       */
      var banner;

      /**
       * Fake blocking issue with an optional action created before
       * each test.
       * @type {media_router.Issue}
       */
      var fakeBlockingIssueOne;

      /**
       * Fake blocking issue without an optional action created before
       * each test.
       * @type {media_router.Issue}
       */
      var fakeBlockingIssueTwo;

      /**
       * Fake non-blocking issue with an optional action created before
       * each test.
       * @type {media_router.Issue}
       */
      var fakeNonBlockingIssueOne;

      /**
       * Fake non-blocking issue without an optional action created before
       * each test.
       * @type {media_router.Issue}
       */
      var fakeNonBlockingIssueTwo;

      // Checks whether the 'issue-action-click' event was fired with the
      // expected data.
      var checkDataFromEventFiring = function(issue, data, isDefault) {
        assertEquals(issue.id, data.detail.id);
        if (isDefault)
          assertEquals(issue.defaultActionType, data.detail.actionType);
        else
          assertEquals(issue.secondaryActionType, data.detail.actionType);
        assertEquals(issue.helpPageId, data.detail.helpPageId);
      };

      // Checks whether |expected| and the text in the |elementId| element
      // are equal.
      var checkElementText = function(expected, elementId) {
        assertEquals(expected.trim(), banner.$[elementId].textContent.trim());
      };

      // Checks whether |issue| message are equal with the message text in the
      // UI.
      var checkIssueText = function(issue) {
        if (issue) {
          checkElementText(issue.message, 'blocking-issue-message');
          checkElementText(issue.message, 'non-blocking-message');

          checkElementText(loadTimeData.getString(
              banner.issueActionTypeToButtonTextResource_[
                  issue.defaultActionType]), 'blocking-default');
          checkElementText(loadTimeData.getString(
              banner.issueActionTypeToButtonTextResource_[
                  issue.defaultActionType]), 'non-blocking-default');

          if (issue.secondaryActionType) {
            checkElementText(loadTimeData.getString(
                banner.issueActionTypeToButtonTextResource_[
                    issue.secondaryActionType]), 'blocking-opt');
            checkElementText(loadTimeData.getString(
                banner.issueActionTypeToButtonTextResource_[
                    issue.secondaryActionType]), 'non-blocking-opt');
          }
        } else {
          checkElementText('', 'blocking-issue-message');
          checkElementText('', 'non-blocking-message');
          checkElementText('', 'blocking-default');
          checkElementText('', 'non-blocking-default');
        }
      };

      // Checks whether parts of the UI is visible.
      var checkVisibility = function(isBlocking, hasOptAction) {
        assertEquals(!isBlocking, banner.$['blocking-issue'].hidden);
        assertEquals(isBlocking, banner.$['non-blocking-issue'].hidden);
        if (isBlocking) {
          assertEquals(!hasOptAction, banner.$['blocking-issue-buttons']
              .querySelector('paper-button').hidden);
        } else {
          assertEquals(!hasOptAction, banner.$['non-blocking-issue']
              .querySelector('span').hidden);
        }
      };

      // Import issue_banner.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://media-router/elements/issue_banner/' +
            'issue_banner.html');
      });

      // Initialize an issue-banner before each test.
      setup(function(done) {
        PolymerTest.clearBody();
        banner = document.createElement('issue-banner');
        document.body.appendChild(banner);

        // Initialize issues.
        fakeBlockingIssueOne = new media_router.Issue(
            'issue id 1', 'Issue Title 1', 'Issue Message 1', 0, 1,
            'route id 1', true, 1234);
        fakeBlockingIssueTwo = new media_router.Issue(
            'issue id 2', 'Issue Title 2', 'Issue Message 2', 0, null,
            'route id 2', true, 1234);
        fakeNonBlockingIssueOne = new media_router.Issue(
            'issue id 3', 'Issue Title 3', 'Issue Message 3', 0, 1,
            'route id 3', false, 1234);
        fakeNonBlockingIssueTwo = new media_router.Issue(
            'issue id 4', 'Issue Title 4', 'Issue Message 4', 0, null,
            'route id 4', false, 1234);

        // Allow for the issue banner to be created and attached.
        setTimeout(done);
      });

      // Tests for 'issue-action-click' event firing when a blocking issue
      // default action is clicked.
      test('blocking issue default action click', function(done) {
        banner.issue = fakeBlockingIssueOne;
        banner.addEventListener('issue-action-click', function(data) {
          checkDataFromEventFiring(fakeBlockingIssueOne,
              data, true);
          done();
        });
        MockInteractions.tap(banner.$['blocking-default']);
      });

      // Tests for 'issue-action-click' event firing when a blocking issue
      // optional action is clicked.
      test('blocking issue optional action click', function(done) {
        banner.issue = fakeBlockingIssueOne;
        banner.addEventListener('issue-action-click', function(data) {
          checkDataFromEventFiring(fakeBlockingIssueOne,
              data, false);
          done();
        });
        MockInteractions.tap(banner.$['blocking-opt']);
      });

      // Tests for 'issue-action-click' event firing when a non-blocking issue
      // default action is clicked.
      test('non-blocking issue default action click', function(done) {
        banner.issue = fakeNonBlockingIssueOne;
        banner.addEventListener('issue-action-click', function(data) {
          checkDataFromEventFiring(fakeNonBlockingIssueOne,
              data, true);
          done();
        });
        MockInteractions.tap(banner.$['non-blocking-default']);
      });

      // Tests for 'issue-action-click' event firing when a non-blocking issue
      // optional action is clicked.
      test('non-blocking issue optional action click', function(done) {
        banner.issue = fakeNonBlockingIssueOne;
        banner.addEventListener('issue-action-click', function(data) {
          checkDataFromEventFiring(fakeNonBlockingIssueOne,
              data, false);
          done();
        });
        MockInteractions.tap(banner.$['non-blocking-opt']);
      });

      // Tests the issue text. While the UI will show only the blocking or
      // non-blocking interface, the issue's info will be set if specified.
      test('issue text', function() {
        // |issue| is null. Title and message text should be empty.
        assertEquals(null, banner.issue);
        checkIssueText(banner.issue);

        // Set |issue| to be a blocking issue. Title and message text should
        // be updated.
        banner.issue = fakeBlockingIssueOne;
        checkIssueText(banner.issue);

        // Set |issue| to be a non-blocking issue. Title and message text
        // should be updated.
        banner.issue = fakeNonBlockingIssueOne;
        checkIssueText(banner.issue);
      });

      // Tests whether parts of the issue-banner is hidden based on the
      // current state.
      test('hidden versus visible components', function() {
        // The blocking UI should be shown along with an optional action.
        banner.issue = fakeBlockingIssueOne;
        checkVisibility(fakeBlockingIssueOne.isBlocking,
            fakeBlockingIssueOne.secondaryActionType);

        // The blocking UI should be shown with no an optional action.
        banner.issue = fakeBlockingIssueTwo;
        checkVisibility(fakeBlockingIssueTwo.isBlocking,
            fakeBlockingIssueTwo.secondaryActionType);

        // The non-blocking UI should be shown along with an optional action.
        banner.issue = fakeNonBlockingIssueOne;
        checkVisibility(fakeNonBlockingIssueOne.isBlocking,
            fakeNonBlockingIssueOne.secondaryActionType);

        // The non-blocking UI should be shown with no an optional action.
        banner.issue = fakeNonBlockingIssueTwo;
        checkVisibility(fakeNonBlockingIssueTwo.isBlocking,
            fakeNonBlockingIssueTwo.secondaryActionType);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
