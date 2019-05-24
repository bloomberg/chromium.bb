/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import './cp-loading.js';
import './error-set.js';
import '@polymer/polymer/lib/elements/dom-if.js';
import AlertsControls from './alerts-controls.js';
import AlertsRequest from './alerts-request.js';
import AlertsTable from './alerts-table.js';
import ChartCompound from './chart-compound.js';
import ChartTimeseries from './chart-timeseries.js';
import ExistingBugRequest from './existing-bug-request.js';
import MenuInput from './menu-input.js';
import NewBugRequest from './new-bug-request.js';
import TriageExisting from './triage-existing.js';
import TriageNew from './triage-new.js';
import groupAlerts from './group-alerts.js';
import {ElementBase, STORE} from './element-base.js';
import {UPDATE} from './simple-redux.js';
import {get} from '@polymer/polymer/lib/utils/path.js';
import {html} from '@polymer/polymer/polymer-element.js';

import {
  BatchIterator,
  animationFrame,
  plural,
  setImmutable,
  simpleGUID,
  transformAlert,
  timeout,
} from './utils.js';

const NOTIFICATION_MS = 5000;

// loadMore() below chases cursors when loading untriaged alerts until it's
// loaded enough alert groups and spent enough time waiting for the backend.
const ENOUGH_GROUPS = 100;
const ENOUGH_LOADING_MS = 60000;

export default class AlertsSection extends ElementBase {
  static get is() { return 'alerts-section'; }

  static get properties() {
    return {
      statePath: String,
      linkedStatePath: String,
      ...AlertsTable.properties,
      ...AlertsControls.properties,
      existingBug: Object,
      isLoading: Boolean,
      newBug: Object,
      preview: Object,
      sectionId: Number,
      selectedAlertPath: String,
      totalCount: Number,
    };
  }

  static buildState(options = {}) {
    return {
      ...AlertsTable.buildState(options),
      ...AlertsControls.buildState(options),
      existingBug: TriageExisting.buildState({}),
      isLoading: false,
      newBug: TriageNew.buildState({}),
      preview: ChartCompound.buildState(options),
      sectionId: options.sectionId || simpleGUID(),
      selectedAlertPath: undefined,
      totalCount: 0,
    };
  }

  static get template() {
    return html`
      <style>
        #triage_controls {
          align-items: center;
          display: flex;
          padding-left: 24px;
          transition: background-color var(--transition-short, 0.2s),
                      color var(--transition-short, 0.2s);
        }

        #triage_controls[anySelected] {
          background-color: var(--primary-color-light, lightblue);
          color: var(--primary-color-dark, blue);
        }

        #triage_controls .button {
          background: unset;
          cursor: pointer;
          font-weight: bold;
          padding: 8px;
          text-transform: uppercase;
        }

        #triage_controls .button[disabled] {
          color: var(--neutral-color-dark, grey);
          font-weight: normal;
        }

        #count {
          flex-grow: 1;
        }
      </style>

      <alerts-controls
          id="controls"
          state-path="[[statePath]]"
          on-sources="onSources_">
      </alerts-controls>

      <error-set errors="[[errors]]"></error-set>
      <cp-loading loading$="[[isLoading_(isLoading, preview.isLoading)]]">
      </cp-loading>

      <template is="dom-if" if="[[!isEmpty_(alertGroups)]]">
        <div id="triage_controls"
            anySelected$="[[!isEqual_(0, selectedAlertsCount)]]">
          <div id="count">
            [[selectedAlertsCount]] selected of
            [[summary_(showingTriaged, alertGroups)]]
          </div>

          <span style="position: relative;">
            <div class="button"
                disabled$="[[!canTriage_(alertGroups)]]"
                on-click="onTriageNew_">
              New Bug
            </div>

            <triage-new
                tabindex="0"
                state-path="[[statePath]].newBug"
                on-submit="onTriageNewSubmit_">
            </triage-new>
          </span>

          <span style="position: relative;">
            <div class="button"
                disabled$="[[!canTriage_(alertGroups)]]"
                on-click="onTriageExisting_">
              Existing Bug
            </div>

            <triage-existing
                tabindex="0"
                state-path="[[statePath]].existingBug"
                on-submit="onTriageExistingSubmit_">
            </triage-existing>
          </span>

          <div class="button"
              disabled$="[[!canTriage_(alertGroups)]]"
              on-click="onIgnore_">
            Ignore
          </div>

          <div class="button"
              disabled$="[[!canUnassignAlerts_(alertGroups)]]"
              on-click="onUnassign_">
            Unassign
          </div>
        </div>
      </template>

      <alerts-table
          state-path="[[statePath]]"
          on-selected="onSelected_"
          on-alert-click="onAlertClick_">
      </alerts-table>

      <iron-collapse opened="[[!allTriaged_(alertGroups, showingTriaged)]]">
        <chart-compound
            id="preview"
            state-path="[[statePath]].preview"
            linked-state-path="[[linkedStatePath]]"
            on-line-count-change="onPreviewLineCountChange_">
          Select alerts using the checkboxes in the table above to preview
          their timeseries.
        </chart-compound>
      </iron-collapse>
    `;
  }

  ready() {
    super.ready();
    this.scrollIntoView(true);
  }

  isLoading_(isLoading, isPreviewLoading) {
    return isLoading || isPreviewLoading;
  }

  summary_(showingTriaged, alertGroups) {
    return AlertsSection.summary(
        showingTriaged, alertGroups, this.totalCount);
  }

  allTriaged_(alertGroups, showingTriaged) {
    if (!alertGroups) return true;
    if (showingTriaged) return alertGroups.length === 0;
    return alertGroups.filter(group =>
      group.alerts.length > group.triaged.count).length === 0;
  }

  canTriage_(alertGroups) {
    if (!window.IS_PRODUCTION) return false;
    const selectedAlerts = AlertsTable.getSelectedAlerts(alertGroups);
    if (selectedAlerts.length === 0) return false;
    for (const alert of selectedAlerts) {
      if (alert.bugId) return false;
    }
    return true;
  }

  canUnassignAlerts_(alertGroups) {
    const selectedAlerts = AlertsTable.getSelectedAlerts(alertGroups);
    for (const alert of selectedAlerts) {
      if (alert.bugId) return true;
    }
    return false;
  }

  async onSources_(event) {
    await AlertsSection.loadAlerts(this.statePath, event.detail.sources);
  }

  async onUnassign_(event) {
    await AlertsSection.changeBugId(this.statePath, 0);
  }

  onTriageNew_(event) {
    // If the user is already signed in, then require-sign-in will do nothing,
    // and openNewBugDialog will do so. If the user is not already signed in,
    // then openNewBugDialog won't, and require-sign-in will start the signin
    // flow. Users can retry triaging after completing the signin flow.
    this.dispatchEvent(new CustomEvent('require-sign-in', {
      bubbles: true,
      composed: true,
    }));
    AlertsSection.openNewBugDialog(this.statePath);
  }

  onTriageExisting_(event) {
    // If the user is already signed in, then require-sign-in will do nothing,
    // and openExistingBugDialog will do so. If the user is not already signed
    // in, then openExistingBugDialog won't, and require-sign-in will start
    // the signin flow.
    this.dispatchEvent(new CustomEvent('require-sign-in', {
      bubbles: true,
      composed: true,
    }));
    AlertsSection.openExistingBugDialog(this.statePath);
  }

  onTriageNewSubmit_(event) {
    AlertsSection.submitNewBug(this.statePath);
  }

  onTriageExistingSubmit_(event) {
    AlertsSection.submitExistingBug(this.statePath);
  }

  onIgnore_(event) {
    AlertsSection.ignore(this.statePath);
  }

  onSelected_(event) {
    AlertsSection.maybeLayoutPreview(this.statePath);
  }

  onAlertClick_(event) {
    STORE.dispatch({
      type: AlertsSection.reducers.selectAlert.name,
      statePath: this.statePath,
      alertGroupIndex: event.detail.alertGroupIndex,
      alertIndex: event.detail.alertIndex,
    });
  }

  onPreviewLineCountChange_() {
    STORE.dispatch({
      type: AlertsSection.reducers.updateAlertColors.name,
      statePath: this.statePath,
    });
  }

  static storeRecentlyModifiedBugs(statePath) {
    const state = get(STORE.getState(), statePath);
    localStorage.setItem('recentlyModifiedBugs', JSON.stringify(
        state.recentlyModifiedBugs));
  }

  static async submitExistingBug(statePath) {
    METRICS.endTriage();
    METRICS.startTriage();
    let state = get(STORE.getState(), statePath);
    const triagedBugId = state.existingBug.bugId;
    STORE.dispatch(UPDATE(`${statePath}.existingBug`, {isOpen: false}));
    await AlertsSection.changeBugId(statePath, triagedBugId);
    STORE.dispatch({
      type: AlertsSection.reducers.showTriagedExisting.name,
      statePath,
      triagedBugId,
    });
    await AlertsSection.storeRecentlyModifiedBugs(statePath);

    // showTriagedExisting sets hasTriagedNew and triagedBugId, causing
    // alerts-controls to display a notification. Wait a few seconds for the
    // user to notice the notification, then automatically hide it. The user
    // will still be able to access the bug by clicking Recent Bugs in
    // alerts-controls.
    await timeout(NOTIFICATION_MS);
    state = get(STORE.getState(), statePath);
    if (!state || (state.triagedBugId !== triagedBugId)) return;
    STORE.dispatch(UPDATE(statePath, {
      hasTriagedExisting: false,
      triagedBugId: 0,
    }));
  }

  static async changeBugId(statePath, bugId) {
    STORE.dispatch(UPDATE(statePath, {isLoading: true}));
    const rootState = STORE.getState();
    let state = get(rootState, statePath);
    const selectedAlerts = AlertsTable.getSelectedAlerts(
        state.alertGroups);
    const alertKeys = new Set(selectedAlerts.map(a => a.key));
    try {
      const request = new ExistingBugRequest({alertKeys, bugId});
      await request.response;
      STORE.dispatch({
        type: AlertsSection.reducers.removeOrUpdateAlerts.name,
        statePath,
        alertKeys,
        bugId,
      });

      state = get(STORE.getState(), statePath);
      if (bugId !== 0) {
        STORE.dispatch(UPDATE(`${statePath}.preview`, {lineDescriptors: []}));
      }
    } catch (err) {
      // eslint-disable-next-line no-console
      console.error(err);
    }
    STORE.dispatch(UPDATE(statePath, {isLoading: false}));
  }

  static async ignore(statePath) {
    let state = get(STORE.getState(), statePath);
    const alerts = AlertsTable.getSelectedAlerts(state.alertGroups);
    const ignoredCount = alerts.length;
    await AlertsSection.changeBugId(statePath, -2);

    STORE.dispatch(UPDATE(statePath, {
      hasTriagedExisting: false,
      hasTriagedNew: false,
      hasIgnored: true,
      ignoredCount,
    }));

    // Setting hasIgnored and ignoredCount causes alerts-controls to display a
    // notification. Wait a few seconds for the user to notice the
    // notification, then automatically hide it. The user can still access
    // ignored alerts by toggling New Only to New and Triaged in
    // alerts-controls.
    await timeout(NOTIFICATION_MS);
    state = get(STORE.getState(), statePath);
    if (!state || (state.ignoredCount !== ignoredCount)) return;
    STORE.dispatch(UPDATE(statePath, {
      hasIgnored: false,
      ignoredCount: 0,
    }));
  }

  static async openNewBugDialog(statePath) {
    let userEmail = STORE.getState().userEmail;
    if (window.IS_DEBUG) {
      userEmail = 'you@chromium.org';
    }
    if (!userEmail) return;
    STORE.dispatch({
      type: AlertsSection.reducers.openNewBugDialog.name,
      statePath,
      userEmail,
    });
  }

  static async openExistingBugDialog(statePath) {
    let userEmail = STORE.getState().userEmail;
    if (window.IS_DEBUG) {
      userEmail = 'you@chromium.org';
    }
    if (!userEmail) return;
    STORE.dispatch({
      type: AlertsSection.reducers.openExistingBugDialog.name,
      statePath,
    });
  }

  static async submitNewBug(statePath) {
    METRICS.endTriage();
    METRICS.startTriage();
    STORE.dispatch(UPDATE(statePath, {isLoading: true}));
    const rootState = STORE.getState();
    let state = get(rootState, statePath);
    const selectedAlerts = AlertsTable.getSelectedAlerts(
        state.alertGroups);
    const alertKeys = new Set(selectedAlerts.map(a => a.key));
    STORE.dispatch({
      type: AlertsSection.reducers.removeOrUpdateAlerts.name,
      statePath,
      alertKeys,
      bugId: '[creating]',
    });

    let bugId;
    try {
      const request = new NewBugRequest({
        alertKeys,
        ...state.newBug,
        labels: state.newBug.labels.filter(
            x => x.isEnabled).map(x => x.name),
        components: state.newBug.components.filter(
            x => x.isEnabled).map(x => x.name),
      });
      const summary = state.newBug.summary;
      bugId = await request.response;
      STORE.dispatch({
        type: AlertsSection.reducers.showTriagedNew.name,
        statePath,
        bugId,
        summary,
      });
      await AlertsSection.storeRecentlyModifiedBugs(statePath);

      STORE.dispatch({
        type: AlertsSection.reducers.removeOrUpdateAlerts.name,
        statePath,
        alertKeys,
        bugId,
      });
      state = get(STORE.getState(), statePath);
      STORE.dispatch(UPDATE(`${statePath}.preview`, {lineDescriptors: []}));
    } catch (err) {
      // eslint-disable-next-line no-console
      console.error(err);
    }
    STORE.dispatch(UPDATE(statePath, {isLoading: false}));

    if (bugId === undefined) return;

    // showTriagedNew sets hasTriagedNew and triagedBugId, causing
    // alerts-controls to display a notification. Wait a few seconds for the
    // user to notice the notification, then automatically hide it. The user
    // will still be able to access the new bug by clicking Recent Bugs in
    // alerts-controls.
    await timeout(NOTIFICATION_MS);
    state = get(STORE.getState(), statePath);
    if (!state || state.triagedBugId !== bugId) return;
    STORE.dispatch(UPDATE(statePath, {
      hasTriagedNew: false,
      triagedBugId: 0,
    }));
  }

  static async loadAlerts(statePath, sources) {
    const started = performance.now();
    STORE.dispatch({
      type: AlertsSection.reducers.startLoadingAlerts.name,
      statePath,
      started,
    });
    if (sources.length) {
      MenuInput.blurAll();
    } else {
      AlertsSection.maybeLayoutPreview(statePath);
    }

    // When a request for untriaged alerts finishes, a request is started for
    // overlapping triaged alerts. This is used to avoid fetching the same
    // triaged alerts multiple times.
    let triagedMaxStartRevision;

    METRICS.startLoadAlerts();

    // Use a BatchIterator to batch AlertsRequest.response.
    // Each batch of results is handled in handleBatch(), then displayed by
    // dispatching reducers.receiveAlerts.
    // loadMore() may add more AlertsRequests to the BatchIterator to chase
    // datastore query cursors.
    const batches = new BatchIterator(sources.map(wrapRequest));

    // Wait one tick to let the browser start the first AlertsRequests, then,
    // while waiting for the server to respond, initialize the memory
    // relationships.
    await Promise.resolve();
    d.getMemoryRelatedNames();

    for await (const {results, errors} of batches) {
      let state = get(STORE.getState(), statePath);
      if (!state || state.started !== started) {
        // Abandon this loadAlerts if the section was closed or if
        // loadAlerts() was called again before this one finished.
        return;
      }

      const {alerts, nextRequests, triagedRequests, totalCount} = handleBatch(
          results, state.showingTriaged);
      if (alerts.length || errors.length) {
        STORE.dispatch({
          type: AlertsSection.reducers.receiveAlerts.name,
          statePath,
          alerts,
          errors,
          totalCount,
        });
        METRICS.endLoadAlerts();
      }
      state = get(STORE.getState(), statePath);
      if (!state || state.started !== started) return;

      triagedMaxStartRevision = loadMore(
          batches, state.alertGroups, nextRequests, triagedRequests,
          triagedMaxStartRevision, started);
      await animationFrame();
      AlertsSection.maybeLayoutPreview(statePath);
    }

    STORE.dispatch({
      type: AlertsSection.reducers.finalizeAlerts.name,
      statePath,
    });
  }

  static async layoutPreview(statePath) {
    const state = get(STORE.getState(), statePath);
    const alerts = AlertsTable.getSelectedAlerts(state.alertGroups);
    const lineDescriptors = alerts.map(AlertsSection.computeLineDescriptor);
    if (lineDescriptors.length === 1) {
      lineDescriptors.push({
        ...lineDescriptors[0],
        buildType: 'ref',
      });
    }
    STORE.dispatch(UPDATE(`${statePath}.preview`, {lineDescriptors}));
  }

  static async maybeLayoutPreview(statePath) {
    const state = get(STORE.getState(), statePath);
    if (!state || !state.selectedAlertsCount) {
      STORE.dispatch(UPDATE(`${statePath}.preview`, {lineDescriptors: []}));
      return;
    }

    AlertsSection.layoutPreview(statePath);
  }
}

async function wrapRequest(body) {
  const request = new AlertsRequest({body});
  const response = await request.response;
  return {body, response};
}

// The BatchIterator in loadAlerts() yielded a batch of results.
// Collect all alerts from all batches into `alerts`.
// Chase cursors in `nextRequests`.
// Fetch triaged alerts when a request for untriaged alerts returns.
function handleBatch(results, showingTriaged) {
  const alerts = [];
  const nextRequests = [];
  const triagedRequests = [];
  let totalCount = 0;
  for (const {body, response} of results) {
    alerts.push.apply(alerts, response.anomalies);

    if (body.count_limit) totalCount += response.count;

    const cursor = response.next_cursor;
    if (cursor) {
      const request = {...body, cursor};
      delete request.count_limit;
      nextRequests.push(request);
    }

    if (!showingTriaged && body.bug_id === '') {
      // Prepare to fetch triaged alerts for the untriaged alerts that
      // were just received.
      const request = {...body, bug_id: '*'};
      delete request.recovered;
      delete request.count_limit;
      delete request.cursor;
      delete request.is_improvement;
      triagedRequests.push(request);
    }
  }

  return {alerts, nextRequests, triagedRequests, totalCount};
}

// This function may add requests to `batches`.
// See handleBatch for `nextRequests` and `triagedRequests`.
function loadMore(batches, alertGroups, nextRequests, triagedRequests,
    triagedMaxStartRevision, started) {
  const minStartRevision = tr.b.math.Statistics.min(
      alertGroups, group => tr.b.math.Statistics.min(
          group.alerts, a => a.startRevision));

  if (!triagedMaxStartRevision ||
      (minStartRevision < triagedMaxStartRevision)) {
    for (const request of triagedRequests) {
      request.min_start_revision = minStartRevision;
      if (triagedMaxStartRevision) {
        request.max_start_revision = triagedMaxStartRevision;
      }
      batches.add(wrapRequest(request));
    }
  }

  for (const next of nextRequests) {
    // Always chase down cursors for triaged alerts.
    // Limit the number of alertGroups displayed to prevent OOM.
    if (next.bug_id === '*' ||
        (alertGroups.length < ENOUGH_GROUPS &&
        ((performance.now() - started) < ENOUGH_LOADING_MS))) {
      batches.add(wrapRequest(next));
    }
  }

  return minStartRevision;
}

AlertsSection.computeLineDescriptor = alert => {
  return {
    baseUnit: alert.baseUnit,
    suites: [alert.suite],
    measurement: alert.measurement,
    bots: [alert.master + ':' + alert.bot],
    cases: [alert.case],
    statistic: alert.statistic,
    buildType: 'test',
  };
};

AlertsSection.reducers = {
  selectAlert: (state, action, rootState) => {
    if (state.alertGroups === AlertsTable.placeholderAlertGroups()) {
      return state;
    }
    const alertPath =
      `alertGroups.${action.alertGroupIndex}.alerts.${action.alertIndex}`;
    const alert = get(state, alertPath);
    if (!alert.isSelected) {
      state = setImmutable(
          state, `${alertPath}.isSelected`, true);
    }
    if (state.selectedAlertPath === alertPath) {
      return {
        ...state,
        selectedAlertPath: undefined,
        preview: {
          ...state.preview,
          lineDescriptors: AlertsTable.getSelectedAlerts(
              state.alertGroups).map(AlertsSection.computeLineDescriptor),
        },
      };
    }
    return {
      ...state,
      selectedAlertPath: alertPath,
      preview: {
        ...state.preview,
        lineDescriptors: [AlertsSection.computeLineDescriptor(alert)],
      },
    };
  },

  showTriagedNew: (state, action, rootState) => {
    return {
      ...state,
      hasTriagedExisting: false,
      hasTriagedNew: true,
      hasIgnored: false,
      triagedBugId: action.bugId,
      recentlyModifiedBugs: [
        {
          id: action.bugId,
          summary: action.summary,
        },
        ...state.recentlyModifiedBugs,
      ],
    };
  },

  showTriagedExisting: (state, action, rootState) => {
    const recentlyModifiedBugs = state.recentlyModifiedBugs.filter(bug =>
      bug.id !== action.triagedBugId);
    let triagedBugSummary = '(TODO fetch bug summary)';
    for (const bug of rootState.recentPerformanceBugs) {
      if (bug.id === action.triagedBugId) {
        triagedBugSummary = bug.summary;
        break;
      }
    }
    recentlyModifiedBugs.unshift({
      id: action.triagedBugId,
      summary: triagedBugSummary,
    });
    return {
      ...state,
      hasTriagedExisting: true,
      hasTriagedNew: false,
      hasIgnored: false,
      triagedBugId: action.triagedBugId,
      recentlyModifiedBugs,
    };
  },

  updateAlertColors: (state, action, rootState) => {
    const colorByDescriptor = new Map();
    for (const line of state.preview.chartLayout.lines) {
      colorByDescriptor.set(ChartTimeseries.stringifyDescriptor(
          line.descriptor), line.color);
    }

    function updateAlert(alert) {
      const descriptor = ChartTimeseries.stringifyDescriptor(
          AlertsSection.computeLineDescriptor(alert));
      const color = colorByDescriptor.get(descriptor);
      return {...alert, color};
    }

    const alertGroups = state.alertGroups.map(alertGroup => {
      const alerts = alertGroup.alerts.map(updateAlert);
      return {...alertGroup, alerts};
    });
    return {...state, alertGroups};
  },

  updateSelectedAlertsCount: state => {
    const selectedAlertsCount = AlertsTable.getSelectedAlerts(
        state.alertGroups).length;
    return {...state, selectedAlertsCount};
  },

  removeAlerts: (state, {alertKeys}, rootState) => {
    const alertGroups = [];
    for (const group of state.alertGroups) {
      const alerts = group.alerts.filter(a => !alertKeys.has(a.key));
      if (alerts.filter(a => !a.bugId).length) {
        alertGroups.push({...group, alerts});
      }
    }
    state = {...state, alertGroups};
    return AlertsSection.reducers.updateSelectedAlertsCount(state);
  },

  updateBugId: (state, {alertKeys, bugId}, rootState) => {
    if (bugId === 0) bugId = '';
    const alertGroups = state.alertGroups.map(alertGroup => {
      const alerts = alertGroup.alerts.map(a =>
        (alertKeys.has(a.key) ? {...a, bugId} : a));
      return {...alertGroup, alerts};
    });
    state = {...state, alertGroups};
    return AlertsSection.reducers.updateSelectedAlertsCount(state);
  },

  removeOrUpdateAlerts: (state, action, rootState) => {
    if (state.showingTriaged || action.bugId === 0) {
      return AlertsSection.reducers.updateBugId(state, action, rootState);
    }
    return AlertsSection.reducers.removeAlerts(state, action, rootState);
  },

  openNewBugDialog: (state, action, rootState) => {
    const alerts = AlertsTable.getSelectedAlerts(state.alertGroups);
    if (alerts.length === 0) return state;
    const newBug = TriageNew.buildState({
      isOpen: true, alerts, cc: action.userEmail,
    });
    return {...state, newBug};
  },

  openExistingBugDialog: (state, action, rootState) => {
    const alerts = AlertsTable.getSelectedAlerts(state.alertGroups);
    if (alerts.length === 0) return state;
    return {
      ...state,
      existingBug: {
        ...state.existingBug,
        ...TriageExisting.buildState({alerts, isOpen: true}),
      },
    };
  },

  receiveAlerts: (state, {alerts, errors, totalCount}, rootState) => {
    errors = errors.map(e => e.message);
    errors = [...new Set([...state.errors, ...errors])];
    state = {...state, errors};

    // |alerts| are all new.
    // Group them together with previously-received alerts from
    // state.alertGroups[].alerts.
    alerts = alerts.map(transformAlert);
    if (state.alertGroups !== AlertsTable.placeholderAlertGroups()) {
      for (const alertGroup of state.alertGroups) {
        alerts.push(...alertGroup.alerts);
      }
    }

    // Automatically select all alerts for bugs.
    if (state.bug.selectedOptions.length > 0 &&
        state.sheriff.selectedOptions.length === 0 &&
        state.report.selectedOptions.length === 0) {
      for (const alert of alerts) {
        alert.isSelected = true;
      }
    }

    if (!alerts.length) {
      return state;
      // Wait till finalizeAlerts to display the happy cat.
    }

    // The user may have already selected and/or triaged some alerts, so keep
    // that information, just re-group the alerts.
    const expandedGroupAlertKeys = new Set();
    const expandedTriagedAlertKeys = new Set();
    for (const group of state.alertGroups) {
      if (group.isExpanded) {
        expandedGroupAlertKeys.add(group.alerts[0].key);
      }
      if (group.triaged.isExpanded) {
        expandedTriagedAlertKeys.add(group.alerts[0].key);
      }
    }

    const groupBugs = state.showingTriaged && (
      state.bug.selectedOptions.length === 1);
    let alertGroups = groupAlerts(alerts, groupBugs);
    alertGroups = alertGroups.map((alerts, groupIndex) => {
      let isExpanded = false;
      let isTriagedExpanded = false;
      for (const a of alerts) {
        if (expandedGroupAlertKeys.has(a.key)) isExpanded = true;
        if (expandedTriagedAlertKeys.has(a.key)) isTriagedExpanded = true;
      }

      return {
        alerts,
        isExpanded,
        triaged: {
          isExpanded: isTriagedExpanded,
          count: alerts.filter(a => a.bugId).length,
        }
      };
    });

    if (!state.showingTriaged && state.sheriff.selectedOptions.length) {
      // Remove completely-triaged groups to save memory.
      alertGroups = alertGroups.filter(group =>
        group.alerts.length > group.triaged.count);
      if (!alertGroups.length) {
        return state;
        // Wait till finalizeAlerts to display the happy cat.
      }
    }

    alertGroups = AlertsTable.sortGroups(
        alertGroups, state.sortColumn, state.sortDescending,
        state.showingTriaged);

    if (totalCount) {
      state = {...state, totalCount};
    }

    // Don't automatically select the first group. Users often want to sort
    // the table by some column before previewing any alerts.

    state = {...state, alertGroups};
    state = AlertsSection.reducers.updateColumns(state);
    state = AlertsSection.reducers.updateSelectedAlertsCount(state);
    return state;
  },

  finalizeAlerts: (state, action, rootState) => {
    state = {...state, isLoading: false};
    if (state.alertGroups === AlertsTable.placeholderAlertGroups() &&
        (state.sheriff.selectedOptions.length ||
          state.bug.selectedOptions.length ||
          state.report.selectedOptions.length)) {
      state = {...state, alertGroups: []};
    }
    return state;
  },

  updateColumns: (state, action, rootState) => {
    // Hide the Triaged, Bug, Master, and Test Case columns if they're boring.
    let showBugColumn = false;
    let showTriagedColumn = false;
    const masters = new Set();
    const cases = new Set();
    for (const group of state.alertGroups) {
      if (group.triaged.count < group.alerts.length) {
        showTriagedColumn = true;
      }
      for (const alert of group.alerts) {
        if (alert.bugId) {
          showBugColumn = true;
        }
        masters.add(alert.master);
        cases.add(alert.case);
      }
    }
    if (state.showingTriaged) showTriagedColumn = false;

    return {
      ...state,
      showBugColumn,
      showMasterColumn: masters.size > 1,
      showTestCaseColumn: cases.size > 1,
      showTriagedColumn,
    };
  },

  startLoadingAlerts: (state, {started}, rootState) => {
    return {
      ...state,
      errors: [],
      alertGroups: AlertsTable.placeholderAlertGroups(),
      isLoading: true,
      started,
      totalCount: 0,
    };
  },
};

AlertsSection.newStateOptionsFromQueryParams = queryParams => {
  return {
    sheriffs: queryParams.getAll('sheriff').map(
        sheriffName => sheriffName.replace(/_/g, ' ')),
    bugs: queryParams.getAll('bug'),
    reports: queryParams.getAll('ar'),
    minRevision: queryParams.get('minRev') || queryParams.get('rev'),
    maxRevision: queryParams.get('maxRev') || queryParams.get('rev'),
    sortColumn: queryParams.get('sort') || 'startRevision',
    showingImprovements: ((queryParams.get('improvements') !== null) ||
      (queryParams.get('bug') !== null)),
    showingTriaged: ((queryParams.get('triaged') !== null) ||
      (queryParams.get('bug') !== null)),
    sortDescending: queryParams.get('descending') !== null,
  };
};

AlertsSection.isEmpty = state => {
  if (!state) return true;
  if (state.sheriff && state.sheriff.selectedOptions &&
      state.sheriff.selectedOptions.length) {
    return false;
  }
  if (state.bug && state.bug.selectedOptions &&
      state.bug.selectedOptions.length) {
    return false;
  }
  if (state.report && state.report.selectedOptions &&
      state.report.selectedOptions.length) {
    return false;
  }
  if (state.minRevision && state.minRevision.match(/^\d+$/)) {
    return false;
  }
  if (state.maxRevision && state.maxRevision.match(/^\d+$/)) {
    return false;
  }
  return true;
};

AlertsSection.getSessionState = state => {
  return {
    sheriffs: state.sheriff.selectedOptions,
    bugs: state.bug.selectedOptions,
    showingImprovements: state.showingImprovements,
    showingTriaged: state.showingTriaged,
    sortColumn: state.sortColumn,
    sortDescending: state.sortDescending,
  };
};

AlertsSection.getRouteParams = state => {
  const queryParams = new URLSearchParams();
  for (const sheriff of state.sheriff.selectedOptions) {
    queryParams.append('sheriff', sheriff.replace(/ /g, '_'));
  }
  for (const bug of state.bug.selectedOptions) {
    queryParams.append('bug', bug);
  }
  for (const name of state.report.selectedOptions) {
    queryParams.append('ar', name);
  }

  const minRev = state.minRevision && state.minRevision.match(/^\d+$/);
  const maxRev = state.maxRevision && state.maxRevision.match(/^\d+$/);
  if ((minRev || maxRev) &&
      !queryParams.get('sheriff') &&
      !queryParams.get('bug') &&
      !queryParams.get('ar')) {
    queryParams.set('alerts', '');
  }
  if (minRev && maxRev && state.minRevision === state.maxRevision) {
    queryParams.set('rev', state.minRevision);
  } else {
    if (minRev) {
      queryParams.set('minRev', state.minRevision);
    }
    if (maxRev) {
      queryParams.set('maxRev', state.maxRevision);
    }
  }

  // #bug implies #improvements and #triaged
  if (state.showingImprovements && !queryParams.get('bug')) {
    queryParams.set('improvements', '');
  }
  if (state.showingTriaged && !queryParams.get('bug')) {
    queryParams.set('triaged', '');
  }
  if (state.sortColumn !== 'startRevision') {
    queryParams.set('sort', state.sortColumn);
  }
  if (state.sortDescending) queryParams.set('descending', '');
  return queryParams;
};

AlertsSection.matchesOptions = (state, options) => {
  if (!options || !state || !state.report || !state.sheriff || !state.bug) {
    return false;
  }
  if (!tr.b.setsEqual(new Set(options.reports),
      new Set(state.report.selectedOptions))) {
    return false;
  }
  if (!tr.b.setsEqual(new Set(options.sheriffs),
      new Set(state.sheriff.selectedOptions))) {
    return false;
  }
  if (!tr.b.setsEqual(new Set(options.bugs),
      new Set(state.bug.selectedOptions))) {
    return false;
  }
  return true;
};

AlertsSection.summary = (showingTriaged, alertGroups, totalCount) => {
  if (!alertGroups ||
      (alertGroups === AlertsTable.placeholderAlertGroups())) {
    return '0 alerts';
  }
  let groupCount = 0;
  let displayedCount = 0;
  for (const group of alertGroups) {
    if (showingTriaged) {
      ++groupCount;
      displayedCount += group.alerts.length;
    } else if (group.alerts.length > group.triaged.count) {
      ++groupCount;
      displayedCount += group.alerts.length - group.triaged.count;
    }
  }
  totalCount = Math.max(totalCount, displayedCount);
  return (
    `${displayedCount} displayed in ` +
    `${groupCount} group${plural(groupCount)} of ` +
    `${totalCount} alert${plural(totalCount)}`);
};

ElementBase.register(AlertsSection);
