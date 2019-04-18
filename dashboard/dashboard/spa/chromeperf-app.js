/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import AlertsSection from './alerts-section.js';
import ChartSection from './chart-section.js';
import ReportSection from './report-section.js';

const NOTIFICATION_MS = 5000;

export default class ChromeperfApp extends cp.ElementBase {
  static get template() {
    return Polymer.html`
      <style>
        chops-header {
          background: var(--background-color, white);
          border-bottom: 1px solid var(--primary-color-medium, blue);
          --chops-header-text-color: var(--primary-color-dark, blue);
        }

        chops-header a {
          color: var(--primary-color-dark, blue);
        }

        iron-icon {
          margin: 0 4px;
        }

        chops-signin {
          margin-left: 16px;
        }

        #body {
          display: flex;
        }
        #drawer {
          background: white;
          border-right: 1px solid var(--primary-color-medium, blue);
          margin-top: 50px;
          position: fixed;
          top: 0;
          bottom: 0;
          user-select: none;
          z-index: var(--layer-drawer, 200);
        }
        #drawer:hover {
          box-shadow: 5px 0 5px 0 rgba(0, 0, 0, 0.2);
        }
        #main {
          overflow: auto;
          position: absolute;
          flex-grow: 1;
          top: 0;
          bottom: 0;
          left: 0;
          right: 0;
        }

        #main[enableNav] {
          left: 32px;
          margin-top: 50px;
        }

        report-section,
        alerts-section,
        chart-section {
          /* Use full-bleed dividers between sections.
            https://material.io/guidelines/components/dividers.html
          */
          border-bottom: 1px solid var(--primary-color-medium);
          display: block;
          margin: 0;
          padding: 16px;
        }

        .nav_button_label {
          display: none;
          margin-right: 8px;
        }
        #drawer:hover .nav_button_label {
          display: inline;
        }
        .drawerbutton {
          align-items: center;
          background-color: var(--background-color, white);
          border: 0;
          cursor: pointer;
          display: flex;
          padding: 8px 0;
          white-space: nowrap;
          width: 100%;
        }
        .drawerbutton:hover {
          background-color: var(--neutral-color-light, lightgrey);
        }
        .drawerbutton[disabled] {
          color: var(--neutral-color-dark, grey);
        }
        .drawerbutton iron-icon {
          color: var(--primary-color-dark, blue);
        }
        .drawerbutton[disabled] iron-icon {
          color: var(--neutral-color-dark, grey);
        }
        a.drawerbutton {
          color: inherit;
          text-decoration: none;
        }

        cp-toast {
          border-radius: 24px;
          cursor: pointer;
          display: flex;
          justify-content: center;
          margin-bottom: 8px;
          padding: 8px 0;
          text-transform: uppercase;
          user-select: none;
          white-space: nowrap;
          width: 100%;
        }

        cp-toast raised-button {
          background-color: var(--primary-color-dark, blue);
          color: var(--background-color, white);
          padding: 8px;
        }

        #old_pages {
          color: var(--foreground-color, black);
          position: relative;
        }
        #old_pages iron-icon {
          margin: 0;
        }
        #old_pages_menu {
          background-color: var(--background-color, white);
          border: 1px solid var(--primary-color-medium, blue);
          display: none;
          flex-direction: column;
          padding: 8px;
          position: absolute;
          width: calc(100% - 16px);
          z-index: var(--layer-menu, 100);
        }
        #old_pages:hover #old_pages_menu {
          display: flex;
        }

        #vulcanized {
          color: grey;
          margin: 16px;
          text-align: right;
        }
      </style>

      <app-route route="{{route}}"></app-route>
      <app-location route="{{route}}" use-hash-as-path></app-location>

      <template is="dom-if" if="[[enableNav]]">
        <chops-header app-title="Chromium Performance">
          <div id="old_pages">
            OLD PAGES <iron-icon icon="cp:more"></iron-icon>
            <div id="old_pages_menu">
              <a target="_blank" href="/alerts">Alerts</a>
              <a target="_blank" href="/report">Charts</a>
            </div>
          </div>

          <template is="dom-if" if="[[isProduction]]">
            <chops-signin></chops-signin>
            <chops-signin-aware on-user-update="onUserUpdate_">
            </chops-signin-aware>
          </template>
        </chops-header>
      </template>

      <cp-loading loading$="[[isLoading]]"></cp-loading>

      <div id="body">
        <div id="drawer" hidden$="[[!enableNav]]">
          <button
              class="drawerbutton"
              id="show_report"
              disabled$="[[showingReportSection]]"
              on-click="onShowReportSection_">
            <iron-icon icon="cp:report"></iron-icon>
            <span class="nav_button_label">Open report</span>
          </button>

          <button
              class="drawerbutton"
              id="new_alerts"
              on-click="onNewAlertsSection_">
            <iron-icon icon="cp:alert"></iron-icon>
            <span class="nav_button_label">New alerts section</span>
          </button>

          <button
              class="drawerbutton"
              id="new_chart"
              on-click="onNewChart_">
            <iron-icon icon="cp:chart"></iron-icon>
            <span class="nav_button_label">New Chart</span>
          </button>

          <button
              class="drawerbutton"
              id="close_charts"
              disabled$="[[isEmpty_(chartSectionIds)]]"
              on-click="onCloseAllCharts_">
            <iron-icon icon="cp:close"></iron-icon>
            <span class="nav_button_label">Close all charts</span>
          </button>

          <a class="drawerbutton"
              href="https://github.com/catapult-project/catapult/tree/master/dashboard"
              target="_blank">
            <iron-icon icon="cp:help"></iron-icon>
            <span class="nav_button_label">Documentation</span>
          </a>

          <template is="dom-if" if="[[isInternal_(userEmail)]]">
            <a class="drawerbutton"
                href="http://go/chromeperf-v2-status"
                target="_blank">
              <iron-icon icon="cp:update"></iron-icon>
              <span class="nav_button_label">Status of this prototype</span>
            </a>
          </template>

          <a class="drawerbutton"
              href$="https://bugs.chromium.org/p/chromium/issues/entry?description=Describe+the+problem:+%0A%0A%0AURL:+[[escapedUrl_(route.path)]]&components=Speed%3EDashboard&summary=[chromeperf2]+"
              target="_blank">
            <iron-icon icon="cp:feedback"></iron-icon>
            <span class="nav_button_label">File a bug</span>
          </a>
        </div>

        <div id="main" enableNav$="[[enableNav]]">
          <template is="dom-if" if="[[reportSection]]">
            <iron-collapse opened="[[showingReportSection]]">
              <report-section
                  state-path="[[statePath]].reportSection"
                  on-require-sign-in="requireSignIn_"
                  on-close-section="hideReportSection_"
                  on-alerts="onReportAlerts_"
                  on-new-chart="onNewChart_">
              </report-section>
            </iron-collapse>
          </template>

          <template is="dom-repeat" items="[[alertsSectionIds]]" as="id">
            <alerts-section
                state-path="[[statePath]].alertsSectionsById.[[id]]"
                linked-state-path="[[statePath]].linkedChartState"
                on-require-sign-in="requireSignIn_"
                on-new-chart="onNewChart_"
                on-close-section="onCloseAlerts_">
            </alerts-section>
          </template>

          <template is="dom-repeat" items="[[chartSectionIds]]" as="id">
            <chart-section
                state-path="[[statePath]].chartSectionsById.[[id]]"
                linked-state-path="[[statePath]].linkedChartState"
                on-require-sign-in="requireSignIn_"
                on-new-chart="onNewChart_"
                on-close-section="onCloseChart_">
            </chart-section>
          </template>

          <div id="vulcanized">
            [[vulcanizedDate]]
          </div>
        </div>
      </div>

      <cp-toast opened="[[!isEmpty_(closedAlertsIds)]]">
        <raised-button id="reopen_alerts" on-click="onReopenClosedAlerts_">
          <iron-icon icon="cp:alert"></iron-icon>
          Reopen alerts
        </raised-button>
      </cp-toast>

      <cp-toast opened="[[!isEmpty_(closedChartIds)]]">
        <raised-button id="reopen_chart" on-click="onReopenClosedChart_">
          <iron-icon icon="cp:chart"></iron-icon>
          Reopen chart
        </raised-button>
      </cp-toast>
    `;
  }

  async ready() {
    super.ready();
    const routeParams = new URLSearchParams(this.route.path);
    this.dispatch('ready', this.statePath, routeParams);
  }

  escapedUrl_(path) {
    return encodeURIComponent(window.location.origin + '#' + path);
  }

  observeReduxRoute_() {
    this.route = {prefix: '', path: this.reduxRoutePath};
  }

  observeAppRoute_() {
    if (!this.readied) return;
    if (this.route.path === '') {
      this.dispatch('reset', this.statePath);
      return;
    }
  }

  async onUserUpdate_() {
    await this.dispatch('userUpdate', this.statePath);
  }

  async onReopenClosedAlerts_(event) {
    await this.dispatch('reopenClosedAlerts', this.statePath);
  }

  async onReopenClosedChart_() {
    await this.dispatch({
      type: ChromeperfApp.reducers.reopenClosedChart.name,
      statePath: this.statePath,
    });
  }

  async requireSignIn_(event) {
    if (this.userEmail || !this.isProduction) return;
    const auth = await window.getAuthInstanceAsync();
    await auth.signIn();
  }

  hideReportSection_(event) {
    this.dispatch(Redux.UPDATE(this.statePath, {
      showingReportSection: false,
    }));
  }

  async onShowReportSection_(event) {
    await this.dispatch(Redux.UPDATE(this.statePath, {
      showingReportSection: true,
    }));
  }

  async onNewAlertsSection_(event) {
    await this.dispatch({
      type: ChromeperfApp.reducers.newAlerts.name,
      statePath: this.statePath,
    });
  }

  async onNewChart_(event) {
    await this.dispatch('newChart', this.statePath, event.detail.options);
  }

  async onCloseChart_(event) {
    await this.dispatch('closeChart', this.statePath, event.model.id);
  }

  async onCloseAlerts_(event) {
    await this.dispatch('closeAlerts', this.statePath, event.model.id);
  }

  async onReportAlerts_(event) {
    await this.dispatch({
      type: ChromeperfApp.reducers.newAlerts.name,
      statePath: this.statePath,
      options: event.detail.options,
    });
  }

  async onCloseAllCharts_(event) {
    await this.dispatch('closeAllCharts', this.statePath);
  }

  observeSections_() {
    if (!this.readied) return;
    this.debounce('updateLocation', () => {
      this.dispatch('updateLocation', this.statePath);
    }, Polymer.Async.animationFrame);
  }

  isInternal_(userEmail) {
    return userEmail.endsWith('@google.com');
  }

  get isProduction() {
    return window.IS_PRODUCTION;
  }
}

ChromeperfApp.State = {
  // App-route sets |route|, and redux sets |reduxRoutePath|.
  // ChromeperfApp translates between them.
  // https://stackoverflow.com/questions/41440316
  reduxRoutePath: options => '#',
  vulcanizedDate: options => options.vulcanizedDate,
  enableNav: options => true,
  isLoading: options => true,
  readied: options => false,

  reportSection: options => ReportSection.buildState({
    sources: [cp.ReportControls.DEFAULT_NAME],
  }),
  showingReportSection: options => true,

  alertsSectionIds: options => [],
  alertsSectionsById: options => {return {};},
  closedAlertsIds: options => [],

  linkedChartState: options => cp.buildState(
      cp.ChartCompound.LinkedState, {}),

  chartSectionIds: options => [],
  chartSectionsById: options => {return {};},
  closedChartIds: options => [],
};

ChromeperfApp.properties = {
  ...cp.buildProperties('state', ChromeperfApp.State),
  route: {type: Object},
  userEmail: {statePath: 'userEmail'},
};

ChromeperfApp.observers = [
  'observeReduxRoute_(reduxRoutePath)',
  'observeAppRoute_(route)',
  ('observeSections_(showingReportSection, reportSection, ' +
    'alertsSectionsById, chartSectionsById)'),
];

ChromeperfApp.actions = {
  ready: (statePath, routeParams) =>
    async(dispatch, getState) => {
      ChromeperfApp.actions.getRevisionInfo()(dispatch, getState);

      dispatch(Redux.CHAIN(
          Redux.ENSURE(statePath),
          Redux.ENSURE('userEmail', ''),
          Redux.ENSURE('largeDom', false),
      ));

      // Wait for ChromeperfApp and its reducers to be registered.
      await cp.afterRender();

      dispatch({
        type: ChromeperfApp.reducers.ready.name,
        statePath,
      });

      if (window.IS_PRODUCTION) {
        // Wait for gapi.auth2 to load and get an Authorization token.
        await window.getAuthInstanceAsync();
      }

      // Now, if the user is signed in, we can get auth headers. Try to
      // restore session state, which might include internal data.
      await ChromeperfApp.actions.restoreFromRoute(
          statePath, routeParams)(dispatch, getState);

      // The app is done loading.
      dispatch(Redux.UPDATE(statePath, {
        isLoading: false,
        readied: true,
      }));
    },

  closeAlerts: (statePath, sectionId) => async(dispatch, getState) => {
    dispatch({
      type: ChromeperfApp.reducers.closeAlerts.name,
      statePath,
      sectionId,
    });
    ChromeperfApp.actions.updateLocation(statePath)(dispatch, getState);

    await cp.timeout(NOTIFICATION_MS);
    const state = Polymer.Path.get(getState(), statePath);
    if (!state.closedAlertsIds.includes(sectionId)) {
      // This alerts section was reopened.
      return;
    }
    dispatch({
      type: ChromeperfApp.reducers.forgetClosedAlerts.name,
      statePath,
    });
  },

  reopenClosedAlerts: statePath => async(dispatch, getState) => {
    const state = Polymer.Path.get(getState(), statePath);
    dispatch(Redux.UPDATE(statePath, {
      alertsSectionIds: [
        ...state.alertsSectionIds,
        ...state.closedAlertsIds,
      ],
      closedAlertsIds: [],
    }));
  },

  userUpdate: statePath => async(dispatch, getState) => {
    const profile = await window.getUserProfileAsync();
    dispatch(Redux.UPDATE('', {
      userEmail: profile ? profile.getEmail() : '',
    }));
    ChromeperfApp.actions.getRevisionInfo()(dispatch, getState);
    if (profile) ChromeperfApp.actions.getRecentBugs()(dispatch, getState);
  },

  getRevisionInfo: () => async(dispatch, getState) => {
    const revisionInfo = await new cp.ConfigRequest(
        {key: 'revision_info'}).response;
    dispatch(Redux.UPDATE('', {revisionInfo}));
  },

  restoreSessionState: (statePath, sessionId) =>
    async(dispatch, getState) => {
      const request = new cp.SessionStateRequest({sessionId});
      const sessionState = await request.response;

      await dispatch(Redux.CHAIN(
          {
            type: ChromeperfApp.reducers.receiveSessionState.name,
            statePath,
            sessionState,
          },
          {
            type: ChromeperfApp.reducers.updateLargeDom.name,
            appStatePath: statePath,
          }));
      await ReportSection.actions.restoreState(
          `${statePath}.reportSection`, sessionState.reportSection
      )(dispatch, getState);
      await ChromeperfApp.actions.updateLocation(statePath)(
          dispatch, getState);
    },

  restoreFromRoute: (statePath, routeParams) => async(dispatch, getState) => {
    if (routeParams.has('nonav')) {
      dispatch(Redux.UPDATE(statePath, {enableNav: false}));
    }

    const sessionId = routeParams.get('session');
    if (sessionId) {
      await ChromeperfApp.actions.restoreSessionState(
          statePath, sessionId)(dispatch, getState);
      return;
    }

    if (routeParams.get('report') !== null) {
      const options = ReportSection.newStateOptionsFromQueryParams(
          routeParams);
      ReportSection.actions.restoreState(
          `${statePath}.reportSection`, options)(dispatch, getState);
      return;
    }

    if (routeParams.get('alerts') !== null ||
        routeParams.get('sheriff') !== null ||
        routeParams.get('bug') !== null ||
        routeParams.get('ar') !== null) {
      const options = AlertsSection.newStateOptionsFromQueryParams(
          routeParams);
      // Hide the report section and create a single alerts-section.
      dispatch(Redux.CHAIN(
          Redux.UPDATE(statePath, {showingReportSection: false}),
          {
            type: ChromeperfApp.reducers.newAlerts.name,
            statePath,
            options,
          },
      ));
      return;
    }

    if (routeParams.get('testSuite') !== null ||
        routeParams.get('suite') !== null ||
        routeParams.get('chart') !== null) {
      // Hide the report section and create a single chart.
      const options = ChartSection.newStateOptionsFromQueryParams(
          routeParams);
      dispatch(Redux.UPDATE(statePath, {showingReportSection: false}));
      ChromeperfApp.actions.newChart(statePath, options)(dispatch, getState);
      return;
    }
  },

  saveSession: statePath => async(dispatch, getState) => {
    const state = Polymer.Path.get(getState(), statePath);
    const sessionState = ChromeperfApp.getSessionState(state);
    const request = new cp.SessionIdRequest({sessionState});
    const session = await request.response;
    const reduxRoutePath = new URLSearchParams({session});
    dispatch(Redux.UPDATE(statePath, {reduxRoutePath}));
  },

  // Compute one of 5 styles of route path (the part of the URL after the
  // origin):
  //  1. /#report=... is used when the report-section is showing and there are
  //     no alerts-sections or chart-sections.
  //  2. /#sheriff=... is used when there is a single alerts-section, zero
  //     chart-sections, and the report-section is hidden.
  //  3. /#suite=... is used when there is a single simple chart-section, zero
  //     alerts-sections, and the report-section is hidden.
  //  4. /## is used when zero sections are showing.
  //  5. /#session=... is used otherwise. The full session state is stored on
  //     the server, addressed by its sha2. SessionIdCacheRequest in the
  //     service worker computes and returns the sha2 so this doesn't need to
  //     wait for a round-trip.
  updateLocation: statePath => async(dispatch, getState) => {
    const rootState = getState();
    const state = Polymer.Path.get(rootState, statePath);
    if (!state.readied) return;
    const nonEmptyAlerts = state.alertsSectionIds.filter(id =>
      !AlertsSection.isEmpty(state.alertsSectionsById[id]));
    const nonEmptyCharts = state.chartSectionIds.filter(id =>
      !ChartSection.isEmpty(state.chartSectionsById[id]));

    let routeParams;

    if (!state.showingReportSection &&
        (nonEmptyAlerts.length === 0) &&
        (nonEmptyCharts.length === 0)) {
      routeParams = new URLSearchParams();
    }

    if (state.showingReportSection &&
        (nonEmptyAlerts.length === 0) &&
        (nonEmptyCharts.length === 0)) {
      routeParams = ReportSection.getRouteParams(state.reportSection);
    }

    if (!state.showingReportSection &&
        (nonEmptyAlerts.length === 1) &&
        (nonEmptyCharts.length === 0)) {
      routeParams = AlertsSection.getRouteParams(
          state.alertsSectionsById[nonEmptyAlerts[0]]);
    }

    if (!state.showingReportSection &&
        (nonEmptyAlerts.length === 0) &&
        (nonEmptyCharts.length === 1)) {
      routeParams = ChartSection.getRouteParams(
          state.chartSectionsById[nonEmptyCharts[0]]);
    }

    if (routeParams === undefined) {
      await ChromeperfApp.actions.saveSession(statePath)(dispatch, getState);
      return;
    }

    if (!state.enableNav) {
      routeParams.set('nonav', '');
    }

    // The extra '#' prevents observeAppRoute_ from dispatching reset.
    const reduxRoutePath = routeParams.toString() || '#';
    dispatch(Redux.UPDATE(statePath, {reduxRoutePath}));
  },

  reset: statePath => async(dispatch, getState) => {
    ReportSection.actions.restoreState(`${statePath}.reportSection`, {
      sources: [cp.ReportControls.DEFAULT_NAME]
    })(dispatch, getState);
    dispatch(Redux.CHAIN(
        Redux.UPDATE(statePath, {showingReportSection: true}),
        {type: ChromeperfApp.reducers.closeAllAlerts.name, statePath}));
    ChromeperfApp.actions.closeAllCharts(statePath)(dispatch, getState);
  },

  newChart: (statePath, options) => async(dispatch, getState) => {
    dispatch(Redux.CHAIN(
        {
          type: ChromeperfApp.reducers.newChart.name,
          statePath,
          options,
        },
        {
          type: ChromeperfApp.reducers.updateLargeDom.name,
          appStatePath: statePath,
        },
    ));
  },

  closeChart: (statePath, sectionId) => async(dispatch, getState) => {
    dispatch({
      type: ChromeperfApp.reducers.closeChart.name,
      statePath,
      sectionId,
    });
    ChromeperfApp.actions.updateLocation(statePath)(dispatch, getState);

    await cp.timeout(NOTIFICATION_MS);
    const state = Polymer.Path.get(getState(), statePath);
    if (!state.closedChartIds.includes(sectionId)) {
      // This chart was reopened.
      return;
    }
    dispatch({
      type: ChromeperfApp.reducers.forgetClosedChart.name,
      statePath,
    });
  },

  closeAllCharts: statePath => async(dispatch, getState) => {
    dispatch({
      type: ChromeperfApp.reducers.closeAllCharts.name,
      statePath,
    });
    ChromeperfApp.actions.updateLocation(statePath)(dispatch, getState);
  },

  getRecentBugs: () => async(dispatch, getState) => {
    const bugs = await new cp.RecentBugsRequest().response;
    dispatch({
      type: ChromeperfApp.reducers.receiveRecentBugs.name,
      bugs,
    });
  },
};

ChromeperfApp.reducers = {
  ready: (state, action, rootState) => {
    let vulcanizedDate = 'dev_appserver';
    if (window.VULCANIZED_TIMESTAMP) {
      vulcanizedDate = tr.b.formatDate(new Date(
          VULCANIZED_TIMESTAMP.getTime() - (1000 * 60 * 60 * 7))) + ' PT';
    }
    return cp.buildState(ChromeperfApp.State, {vulcanizedDate});
  },

  newAlerts: (state, {options}, rootState) => {
    for (const alerts of Object.values(state.alertsSectionsById)) {
      // If the user mashes the ALERTS button, don't open copies of the same
      // alerts section.
      if (!AlertsSection.matchesOptions(alerts, options)) continue;
      if (state.alertsSectionIds.includes(alerts.sectionId)) return state;
      return {
        ...state,
        closedAlertsIds: [],
        alertsSectionIds: [
          alerts.sectionId,
          ...state.alertsSectionIds,
        ],
      };
    }

    const sectionId = cp.simpleGUID();
    const newSection = AlertsSection.buildState({sectionId, ...options});
    const alertsSectionsById = {...state.alertsSectionsById};
    alertsSectionsById[sectionId] = newSection;
    state = {...state};
    const alertsSectionIds = Array.from(state.alertsSectionIds);
    alertsSectionIds.push(sectionId);
    return {...state, alertsSectionIds, alertsSectionsById};
  },

  closeAllAlerts: (state, action, rootState) => {
    return {
      ...state,
      alertsSectionIds: [],
      alertsSectionsById: {},
    };
  },

  closeAlerts: (state, {sectionId}, rootState) => {
    const sectionIdIndex = state.alertsSectionIds.indexOf(sectionId);
    const alertsSectionIds = [...state.alertsSectionIds];
    alertsSectionIds.splice(sectionIdIndex, 1);
    let closedAlertsIds = [];
    if (!AlertsSection.isEmpty(
        state.alertsSectionsById[sectionId])) {
      closedAlertsIds = [sectionId];
    }
    return {...state, alertsSectionIds, closedAlertsIds};
  },

  forgetClosedAlerts: (state, action, rootState) => {
    const alertsSectionsById = {...state.alertsSectionsById};
    for (const id of state.closedAlertsIds) {
      delete alertsSectionsById[id];
    }
    return {
      ...state,
      alertsSectionsById,
      closedAlertsIds: [],
    };
  },

  newChart: (state, {options}, rootState) => {
    for (const chart of Object.values(state.chartSectionsById)) {
      // If the user mashes the OPEN CHART button in the alerts-section, for
      // example, don't open multiple copies of the same chart.
      if ((options && options.clone) ||
          !ChartSection.matchesOptions(chart, options)) {
        continue;
      }
      if (state.chartSectionIds.includes(chart.sectionId)) return state;
      return {
        ...state,
        closedChartIds: [],
        chartSectionIds: [
          chart.sectionId,
          ...state.chartSectionIds,
        ],
      };
    }

    const sectionId = cp.simpleGUID();
    const newSection = {
      type: ChartSection.is,
      sectionId,
      ...ChartSection.buildState(options || {}),
    };
    const chartSectionsById = {...state.chartSectionsById};
    chartSectionsById[sectionId] = newSection;
    state = {...state, chartSectionsById};

    const chartSectionIds = Array.from(state.chartSectionIds);
    chartSectionIds.push(sectionId);

    if (chartSectionIds.length === 1 && options) {
      const linkedChartState = cp.buildState(
          cp.ChartCompound.LinkedState, options);
      state = {...state, linkedChartState};
    }
    return {...state, chartSectionIds};
  },

  closeChart: (state, action, rootState) => {
    // Don't remove the section from chartSectionsById until
    // forgetClosedChart.
    const sectionIdIndex = state.chartSectionIds.indexOf(action.sectionId);
    const chartSectionIds = [...state.chartSectionIds];
    chartSectionIds.splice(sectionIdIndex, 1);
    let closedChartIds = [];
    if (!ChartSection.isEmpty(state.chartSectionsById[action.sectionId])) {
      closedChartIds = [action.sectionId];
    }
    return {...state, chartSectionIds, closedChartIds};
  },

  updateLargeDom: (rootState, action, rootStateAgain) => {
    const state = Polymer.Path.get(rootState, action.appStatePath);
    const sectionCount = (
      state.chartSectionIds.length + state.alertsSectionIds.length);
    return {...rootState, largeDom: (sectionCount > 3)};
  },

  closeAllCharts: (state, action, rootState) => {
    return {
      ...state,
      chartSectionIds: [],
      closedChartIds: Array.from(state.chartSectionIds),
    };
  },

  forgetClosedChart: (state, action, rootState) => {
    const chartSectionsById = {...state.chartSectionsById};
    for (const id of state.closedChartIds) {
      delete chartSectionsById[id];
    }
    return {
      ...state,
      chartSectionsById,
      closedChartIds: [],
    };
  },

  reopenClosedChart: (state, action, rootState) => {
    return {
      ...state,
      chartSectionIds: [
        ...state.chartSectionIds,
        ...state.closedChartIds,
      ],
      closedChartIds: [],
    };
  },

  receiveSessionState: (state, {sessionState}, rootState) => {
    state = {
      ...state,
      isLoading: false,
      showingReportSection: sessionState.showingReportSection,
      alertsSectionIds: [],
      alertsSectionsById: {},
      chartSectionIds: [],
      chartSectionsById: {},
    };

    if (sessionState.alertsSections) {
      for (const options of sessionState.alertsSections) {
        state = ChromeperfApp.reducers.newAlerts(state, {options});
      }
    }
    if (sessionState.chartSections) {
      for (const options of sessionState.chartSections) {
        state = ChromeperfApp.reducers.newChart(state, {options});
      }
    }
    return state;
  },

  receiveRecentBugs: (rootState, {bugs}) => {
    const recentPerformanceBugs = bugs && bugs.map(bug => {
      let revisionRange = bug.summary.match(/.* (\d+):(\d+)$/);
      if (revisionRange === null) {
        revisionRange = new tr.b.math.Range();
      } else {
        revisionRange = tr.b.math.Range.fromExplicitRange(
            parseInt(revisionRange[1]), parseInt(revisionRange[2]));
      }
      return {
        id: '' + bug.id,
        status: bug.status,
        owner: bug.owner ? bug.owner.name : '',
        summary: cp.breakWords(bug.summary),
        revisionRange,
      };
    });
    return {...rootState, recentPerformanceBugs};
  },
};

ChromeperfApp.getSessionState = state => {
  const alertsSections = [];
  for (const id of state.alertsSectionIds) {
    if (AlertsSection.isEmpty(state.alertsSectionsById[id])) continue;
    alertsSections.push(AlertsSection.getSessionState(
        state.alertsSectionsById[id]));
  }
  const chartSections = [];
  for (const id of state.chartSectionIds) {
    if (ChartSection.isEmpty(state.chartSectionsById[id])) continue;
    chartSections.push(ChartSection.getSessionState(
        state.chartSectionsById[id]));
  }

  return {
    enableNav: state.enableNav,
    showingReportSection: state.showingReportSection,
    reportSection: ReportSection.getSessionState(
        state.reportSection),
    alertsSections,
    chartSections,
  };
};

cp.ElementBase.register(ChromeperfApp);
