// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import android.content.Context;
import android.os.Build;
import android.util.DisplayMetrics;

import com.google.protobuf.ByteString;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipInfo.FeatureName;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipSupportedApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionReader;
import org.chromium.chrome.browser.feed.library.api.internal.common.DismissActionWithSemanticProperties;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.FeedRequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.locale.LocaleUtils;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.chrome.browser.feed.library.feedrequestmanager.internal.Utils;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.wire.ActionTypeProto;
import org.chromium.components.feed.core.proto.wire.CapabilityProto.Capability;
import org.chromium.components.feed.core.proto.wire.ClientInfoProto.ClientInfo;
import org.chromium.components.feed.core.proto.wire.ClientInfoProto.ClientInfo.PlatformType;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DisplayInfoProto.DisplayInfo;
import org.chromium.components.feed.core.proto.wire.FeedActionQueryDataProto.Action;
import org.chromium.components.feed.core.proto.wire.FeedActionQueryDataProto.FeedActionQueryData;
import org.chromium.components.feed.core.proto.wire.FeedActionQueryDataProto.FeedActionQueryDataItem;
import org.chromium.components.feed.core.proto.wire.FeedQueryProto.FeedQuery;
import org.chromium.components.feed.core.proto.wire.FeedRequestProto.FeedRequest;
import org.chromium.components.feed.core.proto.wire.RequestProto.Request;
import org.chromium.components.feed.core.proto.wire.RequestProto.Request.RequestVersion;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.components.feed.core.proto.wire.SemanticPropertiesProto.SemanticProperties;
import org.chromium.components.feed.core.proto.wire.VersionProto.Version;
import org.chromium.components.feed.core.proto.wire.VersionProto.Version.Architecture;
import org.chromium.components.feed.core.proto.wire.VersionProto.Version.BuildType;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Default implementation of FeedRequestManager. */
public final class FeedRequestManagerImpl implements FeedRequestManager {
    private static final String TAG = "FeedRequestManagerImpl";

    private final Configuration mConfiguration;
    private final NetworkClient mNetworkClient;
    private final ProtocolAdapter mProtocolAdapter;
    private final FeedExtensionRegistry mExtensionRegistry;
    private final SchedulerApi mScheduler;
    private final TaskQueue mTaskQueue;
    private final TimingUtils mTimingUtils;
    private final ThreadUtils mThreadUtils;
    private final ActionReader mActionReader;
    private final Context mContext;
    private final MainThreadRunner mMainThreadRunner;
    private final BasicLoggingApi mBasicLoggingApi;
    private final TooltipSupportedApi mTooltipSupportedApi;
    private final ApplicationInfo mApplicationInfo;

    public FeedRequestManagerImpl(Configuration configuration, NetworkClient networkClient,
            ProtocolAdapter protocolAdapter, FeedExtensionRegistry extensionRegistry,
            SchedulerApi scheduler, TaskQueue taskQueue, TimingUtils timingUtils,
            ThreadUtils threadUtils, ActionReader actionReader, Context context,
            ApplicationInfo applicationInfo, MainThreadRunner mainThreadRunner,
            BasicLoggingApi basicLoggingApi, TooltipSupportedApi tooltipSupportedApi) {
        this.mConfiguration = configuration;
        this.mNetworkClient = networkClient;
        this.mProtocolAdapter = protocolAdapter;
        this.mExtensionRegistry = extensionRegistry;
        this.mScheduler = scheduler;
        this.mTaskQueue = taskQueue;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtils;
        this.mActionReader = actionReader;
        this.mContext = context;
        this.mApplicationInfo = applicationInfo;
        this.mMainThreadRunner = mainThreadRunner;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mTooltipSupportedApi = tooltipSupportedApi;
    }

    @Override
    public void loadMore(
            StreamToken streamToken, ConsistencyToken token, Consumer<Result<Model>> consumer) {
        mThreadUtils.checkNotMainThread();

        Logger.i(TAG, "Task: FeedRequestManagerImpl LoadMore");
        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        RequestBuilder request = newDefaultRequest(RequestReason.MANUAL_CONTINUATION)
                                         .setPageToken(streamToken.getNextPageToken())
                                         .setConsistencyToken(token);
        executeRequest(request, consumer);
        timeTracker.stop(
                "task", "FeedRequestManagerImpl LoadMore", "token", streamToken.getNextPageToken());
    }

    @Override
    public void triggerRefresh(@RequestReason int reason, Consumer<Result<Model>> consumer) {
        triggerRefresh(reason, ConsistencyToken.getDefaultInstance(), consumer);
    }

    @Override
    public void triggerRefresh(
            @RequestReason int reason, ConsistencyToken token, Consumer<Result<Model>> consumer) {
        Logger.i(TAG, "trigger refresh %s", reason);
        RequestBuilder request = newDefaultRequest(reason).setConsistencyToken(token);

        if (mThreadUtils.isMainThread()) {
            // This will make a new request, it should invalidate the existing head to delay
            // everything until the response is obtained.
            mTaskQueue.execute(Task.REQUEST_MANAGER_TRIGGER_REFRESH, TaskType.HEAD_INVALIDATE,
                    () -> executeRequest(request, consumer));
        } else {
            executeRequest(request, consumer);
        }
    }

    private RequestBuilder newDefaultRequest(@RequestReason int requestReason) {
        return new RequestBuilder(mContext, mApplicationInfo, mConfiguration, requestReason);
    }

    private static FeedQuery.RequestReason getWireRequestReason(@RequestReason int requestReason) {
        switch (requestReason) {
            case RequestReason.ZERO_STATE:
                return FeedQuery.RequestReason.ZERO_STATE_REFRESH;
            case RequestReason.HOST_REQUESTED:
                return FeedQuery.RequestReason.SCHEDULED_REFRESH;
            case RequestReason.OPEN_WITH_CONTENT:
                return FeedQuery.RequestReason.WITH_CONTENT;
                // TODO: distinguish between automatic and manual continuation for wire
                // protocol
            case RequestReason.MANUAL_CONTINUATION:
            case RequestReason.AUTOMATIC_CONTINUATION:
                return FeedQuery.RequestReason.NEXT_PAGE_SCROLL;
            case RequestReason.OPEN_WITHOUT_CONTENT:
                return FeedQuery.RequestReason.INITIAL_LOAD;
            case RequestReason.CLEAR_ALL:
                return FeedQuery.RequestReason.CLEAR_ALL;
            case RequestReason.UNKNOWN:
                return FeedQuery.RequestReason.UNKNOWN_REQUEST_REASON;
            default:
                Logger.wtf(TAG, "Cannot map request reason with value %d", requestReason);
                return FeedQuery.RequestReason.UNKNOWN_REQUEST_REASON;
        }
    }

    private void executeRequest(RequestBuilder requestBuilder, Consumer<Result<Model>> consumer) {
        mThreadUtils.checkNotMainThread();
        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        if (dismissActionsResult.isSuccessful()) {
            requestBuilder.setActions(dismissActionsResult.getValue());
            for (DismissActionWithSemanticProperties dismiss : dismissActionsResult.getValue()) {
                Logger.i(TAG, "Dismiss action: %s", dismiss.getContentId());
            }
        } else {
            Logger.e(TAG, "Error fetching dismiss actions");
        }

        if (mConfiguration.getValueOrDefault(ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE, false)) {
            // We need use the main thread to call the {@link
            // TooltipSupportedApi#wouldTriggerHelpUi}.
            mMainThreadRunner.execute("Check Tooltips", () -> {
                mTooltipSupportedApi.wouldTriggerHelpUi(
                        FeatureName.CARD_MENU_TOOLTIP, (wouldTrigger) -> {
                            mTaskQueue.execute(Task.SEND_REQUEST, TaskType.IMMEDIATE, () -> {
                                requestBuilder.setCardMenuTooltipWouldTrigger(wouldTrigger);
                                sendRequest(requestBuilder, consumer);
                            });
                        });
            });
        } else {
            sendRequest(requestBuilder, consumer);
        }
    }

    private void sendRequest(RequestBuilder requestBuilder, Consumer<Result<Model>> consumer) {
        mThreadUtils.checkNotMainThread();
        String endpoint = mConfiguration.getValueOrDefault(ConfigKey.FEED_SERVER_ENDPOINT, "");
        @HttpMethod
        String httpMethod =
                mConfiguration.getValueOrDefault(ConfigKey.FEED_SERVER_METHOD, HttpMethod.GET);

        HttpRequest httpRequest =
                RequestHelper.buildHttpRequest(httpMethod, requestBuilder.build().toByteArray(),
                        endpoint, LocaleUtils.getLanguageTag(mContext));

        Logger.i(TAG, "Making Request: %s", httpRequest.getUri().getPath());
        mNetworkClient.send(httpRequest, input -> {
            mBasicLoggingApi.onServerRequest(requestBuilder.mClientLoggingRequestReason);
            Logger.i(TAG, "Request: %s completed with response code: %s",
                    httpRequest.getUri().getPath(), input.getResponseCode());
            if (input.getResponseCode() != 200) {
                String errorBody = null;
                try {
                    errorBody = new String(input.getResponseBody(), "UTF-8");
                } catch (UnsupportedEncodingException e) {
                    Logger.e(TAG, "Error handling http error logging", e);
                }
                Logger.e(TAG, "errorCode: %d", input.getResponseCode());
                Logger.e(TAG, "errorResponse: %s", errorBody);
                if (!requestBuilder.hasPageToken()) {
                    mScheduler.onRequestError(input.getResponseCode());
                }
                consumer.accept(Result.failure());
                return;
            }
            handleResponseBytes(input.getResponseBody(), consumer);
        });
    }

    private void handleResponseBytes(
            final byte[] responseBytes, final Consumer<Result<Model>> consumer) {
        mTaskQueue.execute(Task.HANDLE_RESPONSE_BYTES, TaskType.IMMEDIATE, () -> {
            Response response;
            boolean isLengthPrefixed = mConfiguration.getValueOrDefault(
                    ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED, true);
            try {
                response = Response.parseFrom(isLengthPrefixed
                                ? RequestHelper.getLengthPrefixedValue(responseBytes)
                                : responseBytes,
                        mExtensionRegistry.getExtensionRegistry());
            } catch (IOException e) {
                Logger.e(TAG, e, "Response parse failed");
                consumer.accept(Result.failure());
                return;
            }
            mMainThreadRunner.execute("FeedRequestManagerImpl consumer",
                    () -> consumer.accept(mProtocolAdapter.createModel(response)));
        });
    }

    private static final class RequestBuilder {
        private ByteString mToken;
        private ConsistencyToken mConsistencyToken;
        private List<DismissActionWithSemanticProperties> mDismissActionWithSemanticProperties;
        private final Context mContext;
        private final ApplicationInfo mApplicationInfo;
        private final Configuration mConfiguration;
        private final FeedQuery.RequestReason mRequestReason;
        @RequestReason
        private final int mClientLoggingRequestReason;
        private boolean mCardMenuTooltipWouldTrigger;

        RequestBuilder(Context context, ApplicationInfo applicationInfo,
                Configuration configuration, @RequestReason int requestReason) {
            this.mContext = context;
            this.mApplicationInfo = applicationInfo;
            this.mConfiguration = configuration;
            this.mClientLoggingRequestReason = requestReason;
            this.mRequestReason = getWireRequestReason(requestReason);
        }

        /**
         * Sets the token used to tell the server which page of results we want in the response.
         *
         * @param token the token copied from FeedResponse.next_page_token.
         */
        RequestBuilder setPageToken(ByteString token) {
            this.mToken = token;
            return this;
        }

        boolean hasPageToken() {
            return mToken != null;
        }
        /**
         * Sets the token used to tell the server which storage version to read/write to.
         *
         * @param token the token used to maintain read/write consistency.
         */
        RequestBuilder setConsistencyToken(ConsistencyToken token) {
            this.mConsistencyToken = token;
            return this;
        }

        RequestBuilder setActions(
                List<DismissActionWithSemanticProperties> dismissActionsWithSemanticProperties) {
            this.mDismissActionWithSemanticProperties = dismissActionsWithSemanticProperties;
            return this;
        }

        RequestBuilder setCardMenuTooltipWouldTrigger(boolean wouldTrigger) {
            mCardMenuTooltipWouldTrigger = wouldTrigger;
            return this;
        }

        public Request build() {
            Request.Builder requestBuilder =
                    Request.newBuilder().setRequestVersion(RequestVersion.FEED_QUERY);

            FeedQuery.Builder feedQuery = FeedQuery.newBuilder().setReason(mRequestReason);
            if (mToken != null) {
                feedQuery.setPageToken(mToken);
            }
            FeedRequest.Builder feedRequestBuilder =
                    FeedRequest.newBuilder().setFeedQuery(feedQuery);
            if (mConsistencyToken != null) {
                feedRequestBuilder.setConsistencyToken(mConsistencyToken);
                Logger.i(TAG, "Consistency Token: %s", mConsistencyToken);
            }
            if (mDismissActionWithSemanticProperties != null
                    && !mDismissActionWithSemanticProperties.isEmpty()) {
                feedRequestBuilder.addFeedActionQueryData(buildFeedActionQueryData());
            }
            feedRequestBuilder.setClientInfo(buildClientInfo());
            addCapabilities(feedRequestBuilder);
            requestBuilder.setExtension(FeedRequest.feedRequest, feedRequestBuilder.build());

            return requestBuilder.build();
        }

        private void addCapabilities(FeedRequest.Builder feedRequestBuilder) {
            addCapabilityIfConfigEnabled(
                    feedRequestBuilder, ConfigKey.FEED_UI_ENABLED, Capability.FEED_UI);
            addCapabilityIfConfigEnabled(feedRequestBuilder, ConfigKey.UNDOABLE_ACTIONS_ENABLED,
                    Capability.UNDOABLE_ACTIONS);
            addCapabilityIfConfigEnabled(feedRequestBuilder, ConfigKey.MANAGE_INTERESTS_ENABLED,
                    Capability.MANAGE_INTERESTS);
            addCapabilityIfConfigEnabled(
                    feedRequestBuilder, ConfigKey.ENABLE_CAROUSELS, Capability.CAROUSELS);
            if (mCardMenuTooltipWouldTrigger) {
                addCapabilityIfConfigEnabled(feedRequestBuilder,
                        ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE, Capability.CARD_MENU_TOOLTIP);
            }
            addCapabilityIfConfigEnabled(
                    feedRequestBuilder, ConfigKey.SNIPPETS_ENABLED, Capability.ARTICLE_SNIPPETS);
            addCapabilityIfConfigEnabled(feedRequestBuilder, ConfigKey.USE_SECONDARY_PAGE_REQUEST,
                    Capability.USE_SECONDARY_PAGE_REQUEST);
            feedRequestBuilder.addClientCapability(Capability.BASE_UI);

            for (Capability capability : feedRequestBuilder.getClientCapabilityList()) {
                Logger.i(TAG, "Capability: %s", capability.name());
            }
        }

        private void addCapabilityIfConfigEnabled(
                FeedRequest.Builder feedRequestBuilder, String configKey, Capability capability) {
            if (mConfiguration.getValueOrDefault(configKey, false)) {
                feedRequestBuilder.addClientCapability(capability);
            }
        }

        private FeedActionQueryData buildFeedActionQueryData() {
            Map<Long, Integer> ids =
                    new LinkedHashMap<>(mDismissActionWithSemanticProperties.size());
            Map<String, Integer> tables =
                    new LinkedHashMap<>(mDismissActionWithSemanticProperties.size());
            Map<String, Integer> contentDomains =
                    new LinkedHashMap<>(mDismissActionWithSemanticProperties.size());
            Map<SemanticProperties, Integer> semanticProperties =
                    new LinkedHashMap<>(mDismissActionWithSemanticProperties.size());
            ArrayList<FeedActionQueryDataItem> actionDataItems =
                    new ArrayList<>(mDismissActionWithSemanticProperties.size());

            for (DismissActionWithSemanticProperties action :
                    mDismissActionWithSemanticProperties) {
                ContentId contentId = action.getContentId();
                byte /*@Nullable*/[] semanticPropertiesBytes = action.getSemanticProperties();

                FeedActionQueryDataItem.Builder actionDataItemBuilder =
                        FeedActionQueryDataItem.newBuilder();

                actionDataItemBuilder.setIdIndex(getIndexForItem(ids, contentId.getId()));
                actionDataItemBuilder.setTableIndex(getIndexForItem(tables, contentId.getTable()));
                actionDataItemBuilder.setContentDomainIndex(
                        getIndexForItem(contentDomains, contentId.getContentDomain()));
                if (semanticPropertiesBytes != null) {
                    actionDataItemBuilder.setSemanticPropertiesIndex(
                            getIndexForItem(semanticProperties,
                                    SemanticProperties.newBuilder()
                                            .setSemanticPropertiesData(
                                                    ByteString.copyFrom(semanticPropertiesBytes))
                                            .build()));
                }

                actionDataItems.add(actionDataItemBuilder.build());
            }
            return FeedActionQueryData.newBuilder()
                    .setAction(Action.newBuilder().setActionType(
                            ActionTypeProto.ActionType.forNumber(ActionType.DISMISS)))
                    .addAllUniqueId(ids.keySet())
                    .addAllUniqueTable(tables.keySet())
                    .addAllUniqueContentDomain(contentDomains.keySet())
                    .addAllUniqueSemanticProperties(semanticProperties.keySet())
                    .addAllFeedActionQueryDataItem(actionDataItems)
                    .build();
        }

        private static <T> int getIndexForItem(Map<T, Integer> objectMap, T object) {
            if (!objectMap.containsKey(object)) {
                int newIndex = objectMap.size();
                objectMap.put(object, newIndex);
                return newIndex;
            } else {
                return objectMap.get(object);
            }
        }

        private ClientInfo buildClientInfo() {
            ClientInfo.Builder clientInfoBuilder = ClientInfo.newBuilder();
            clientInfoBuilder.setPlatformType(PlatformType.ANDROID_ID);
            clientInfoBuilder.setPlatformVersion(getPlatformVersion());
            clientInfoBuilder.setLocale(LocaleUtils.getLanguageTag(mContext));
            clientInfoBuilder.setAppType(Utils.convertAppType(mApplicationInfo.getAppType()));
            clientInfoBuilder.setAppVersion(getAppVersion());
            DisplayMetrics metrics = mContext.getResources().getDisplayMetrics();
            clientInfoBuilder.addDisplayInfo(
                    DisplayInfo.newBuilder()
                            .setScreenDensity(metrics.density)
                            .setScreenWidthInPixels(metrics.widthPixels)
                            .setScreenHeightInPixels(metrics.heightPixels));
            return clientInfoBuilder.build();
        }

        private static Version getPlatformVersion() {
            Version.Builder builder = Version.newBuilder();
            Utils.fillVersionsFromString(builder, Build.VERSION.RELEASE);
            builder.setArchitecture(getPlatformArchitecture());
            builder.setBuildType(getPlatformBuildType());
            builder.setApiVersion(Build.VERSION.SDK_INT);
            return builder.build();
        }

        private static Architecture getPlatformArchitecture() {
            String androidAbi = Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                    ? Build.SUPPORTED_ABIS[0]
                    : Build.CPU_ABI;
            return Utils.convertArchitectureString(androidAbi);
        }

        private static BuildType getPlatformBuildType() {
            if (Build.TAGS != null) {
                if (Build.TAGS.contains("dev-keys") || Build.TAGS.contains("test-keys")) {
                    return BuildType.DEV;
                } else if (Build.TAGS.contains("release-keys")) {
                    return BuildType.RELEASE;
                }
            }
            return BuildType.UNKNOWN_BUILD_TYPE;
        }

        private Version getAppVersion() {
            Version.Builder builder = Version.newBuilder();
            Utils.fillVersionsFromString(builder, mApplicationInfo.getVersionString());
            builder.setArchitecture(Utils.convertArchitecture(mApplicationInfo.getArchitecture()));
            builder.setBuildType(Utils.convertBuildType(mApplicationInfo.getBuildType()));
            return builder.build();
        }
    }
}
