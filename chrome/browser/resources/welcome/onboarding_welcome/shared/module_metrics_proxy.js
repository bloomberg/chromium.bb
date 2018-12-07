// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  /**
   * NuxEmailProvidersInteractions enum.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   * @enum {number}
   */
  const NuxEmailProvidersInteractions = {
    PageShown: 0,
    DidNothingAndNavigatedAway: 1,
    DidNothingAndChoseSkip: 2,
    ChoseAnOptionAndNavigatedAway: 3,
    ChoseAnOptionAndChoseSkip: 4,
    ChoseAnOptionAndChoseNext: 5,
    ClickedDisabledNextButtonAndNavigatedAway: 6,
    ClickedDisabledNextButtonAndChoseSkip: 7,
    ClickedDisabledNextButtonAndChoseNext: 8,
    DidNothingAndChoseNext: 9,
    NavigatedAwayThroughBrowserHistory: 10,
  };

  /**
   * The number of enum values in NuxEmailProvidersInteractions. This should
   * be kept in sync with the enum count in tools/metrics/histograms/enums.xml.
   * @type {number}
   */
  const INTERACTION_METRIC_COUNT =
      Object.keys(NuxEmailProvidersInteractions).length;

  /** @interface */
  class ModuleMetricsProxy {
    recordPageShown() {}

    recordDidNothingAndNavigatedAway() {}

    recordDidNothingAndChoseSkip() {}

    recordDidNothingAndChoseNext() {}

    recordChoseAnOptionAndNavigatedAway() {}

    recordChoseAnOptionAndChoseSkip() {}

    recordChoseAnOptionAndChoseNext() {}

    recordClickedDisabledNextButtonAndNavigatedAway() {}

    recordClickedDisabledNextButtonAndChoseSkip() {}

    recordClickedDisabledNextButtonAndChoseNext() {}

    recordNavigatedAwayThroughBrowserHistory() {}
  }

  /** @implements {nux.ModuleMetricsProxy} */
  class ModuleMetricsProxyImpl {
    /**
     * @param {string} histogramName The histogram that will record the module
     *      navigation metrics.
     */
    constructor(histogramName) {
      /** @private {string} */
      this.interactionMetric_ = histogramName;
    }

    /** @override */
    recordPageShown() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_, NuxEmailProvidersInteractions.PageShown,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordDidNothingAndNavigatedAway() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.DidNothingAndNavigatedAway,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordDidNothingAndChoseSkip() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.DidNothingAndChoseSkip,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordDidNothingAndChoseNext() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.DidNothingAndChoseNext,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordChoseAnOptionAndNavigatedAway() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.ChoseAnOptionAndNavigatedAway,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordChoseAnOptionAndChoseSkip() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.ChoseAnOptionAndChoseSkip,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordChoseAnOptionAndChoseNext() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.ChoseAnOptionAndChoseNext,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordClickedDisabledNextButtonAndNavigatedAway() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions
              .ClickedDisabledNextButtonAndNavigatedAway,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordClickedDisabledNextButtonAndChoseSkip() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.ClickedDisabledNextButtonAndChoseSkip,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordClickedDisabledNextButtonAndChoseNext() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.ClickedDisabledNextButtonAndChoseNext,
          INTERACTION_METRIC_COUNT);
    }

    /** @override */
    recordNavigatedAwayThroughBrowserHistory() {
      chrome.metricsPrivate.recordEnumerationValue(
          this.interactionMetric_,
          NuxEmailProvidersInteractions.NavigatedAwayThroughBrowserHistory,
          INTERACTION_METRIC_COUNT);
    }
  }

  class ModuleMetricsManager {
    /** @param {nux.ModuleMetricsProxy} metricsProxy */
    constructor(metricsProxy) {
      this.metricsProxy_ = metricsProxy;

      this.options_ = {
        didNothing: {
          andNavigatedAway: metricsProxy.recordDidNothingAndNavigatedAway,
          andChoseSkip: metricsProxy.recordDidNothingAndChoseSkip,
          andChoseNext: metricsProxy.recordDidNothingAndChoseNext,
        },
        choseAnOption: {
          andNavigatedAway: metricsProxy.recordChoseAnOptionAndNavigatedAway,
          andChoseSkip: metricsProxy.recordChoseAnOptionAndChoseSkip,
          andChoseNext: metricsProxy.recordChoseAnOptionAndChoseNext,
        },
        clickedDisabledNextButton: {
          andNavigatedAway:
              metricsProxy.recordClickedDisabledNextButtonAndNavigatedAway,
          andChoseSkip:
              metricsProxy.recordClickedDisabledNextButtonAndChoseSkip,
          andChoseNext:
              metricsProxy.recordClickedDisabledNextButtonAndChoseNext,
        },
      };

      this.firstPart = this.options_.didNothing;
    }

    recordPageInitialized() {
      this.metricsProxy_.recordPageShown();
      this.firstPart = this.options_.didNothing;
    }

    recordClickedOption() {
      // Only overwrite this.firstPart if it's not overwritten already
      if (this.firstPart == this.options_.didNothing)
        this.firstPart = this.options_.choseAnOption;
    }

    recordClickedDisabledButton() {
      // Only overwrite this.firstPart if it's not overwritten already
      if (this.firstPart == this.options_.didNothing)
        this.firstPart = this.options_.clickedDisabledNextButton;
    }

    recordNoThanks() {
      this.firstPart.andChoseSkip.call(this.metricsProxy_);
    }

    recordGetStarted() {
      this.firstPart.andChoseNext.call(this.metricsProxy_);
    }

    recordNavigatedAway() {
      this.firstPart.andNavigatedAway.call(this.metricsProxy_);
    }

    recordBrowserBackOrForward() {
      this.metricsProxy_.recordNavigatedAwayThroughBrowserHistory();
    }
  }

  return {
    ModuleMetricsProxy: ModuleMetricsProxy,
    ModuleMetricsProxyImpl: ModuleMetricsProxyImpl,
    ModuleMetricsManager: ModuleMetricsManager,
  };
});

// This is done outside |cr.define| because the closure compiler wants a fully
// qualified name for |nux.ModuleMetricsProxyImpl|.
nux.EmailMetricsProxyImpl = class extends nux.ModuleMetricsProxyImpl {
  constructor() {
    super('FirstRun.NewUserExperience.EmailProvidersInteraction');
  }
};

cr.addSingletonGetter(nux.EmailMetricsProxyImpl);
