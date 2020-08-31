// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionsResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportsScriptResponseProto;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Test service which serves preconfigured scripts and actions.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantTestService
        implements AutofillAssistantService,
                   AutofillAssistantServiceInjector.NativeServiceProvider {
    private final List<AutofillAssistantTestScript> mScripts;
    private final ClientSettingsProto mClientSettings;
    private List<ActionProto> mNextActions = Collections.emptyList();
    /** The most recently received list of processed actions. */
    private @Nullable List<ProcessedActionProto> mProcessedActions;
    private int mNextActionsCounter;
    private int mCurrentScriptIndex;

    /** Default constructor which disables animations. */
    AutofillAssistantTestService(List<AutofillAssistantTestScript> scripts) {
        this(scripts,
                (ClientSettingsProto) ClientSettingsProto.newBuilder()
                        .setIntegrationTestSettings(
                                ClientSettingsProto.IntegrationTestSettings.newBuilder()
                                        .setDisableHeaderAnimations(true)
                                        .setDisableCarouselChangeAnimations(true))
                        .build());
    }

    /** Constructor which allows injecting custom client settings. */
    AutofillAssistantTestService(
            List<AutofillAssistantTestScript> scripts, ClientSettingsProto clientSettings) {
        mScripts = scripts;
        mClientSettings = clientSettings;
    }

    /**
     * Asks the client to inject this service upon startup. Must be called prior to client startup
     * in order to take effect!
     */
    void scheduleForInjection() {
        AutofillAssistantServiceInjector.setServiceToInject(this);
    }

    /**
     * Sets the actions that will be returned for the next (and only the next) call to {@code
     * getNextActions}.
     */
    void setNextActions(List<ActionProto> nextActions) {
        mNextActions = nextActions;
    }

    @Override
    public long createNativeService() {
        // Ask native to create and return a wrapper around |this|. The wrapper will be injected
        // upon startup, at which point the native controller will take ownership of the wrapper.
        return AutofillAssistantTestServiceJni.get().javaServiceCreate(this);
    }

    /** @see AutofillAssistantService#getScriptsForUrl(String) */
    @Override
    public SupportsScriptResponseProto getScriptsForUrl(String url) {
        // Return scripts one after the other. Note: Returning more than one script at once leads
        // to a dropout with RENDER_PROCESS_GONE.
        SupportsScriptResponseProto.Builder builder = SupportsScriptResponseProto.newBuilder();
        if (mCurrentScriptIndex < mScripts.size()) {
            builder.addScripts(mScripts.get(mCurrentScriptIndex++).getSupportedScript());
        }
        builder.setClientSettings(mClientSettings);
        return builder.build();
    }

    /** @see AutofillAssistantService#getActions(String, String, byte[], byte[]) */
    @Override
    public ActionsResponseProto getActions(
            String scriptPath, String url, byte[] globalPayload, byte[] scriptPayload) {
        for (AutofillAssistantTestScript script : mScripts) {
            if (script.getSupportedScript().getPath().compareTo(scriptPath) != 0) {
                continue;
            }

            return ActionsResponseProto.newBuilder()
                    .addAllActions(script.getActions())
                    .setGlobalPayload(ByteString.copyFrom(globalPayload))
                    .setScriptPayload(ByteString.copyFrom(scriptPayload))
                    .build();
        }

        // Actions requested for non-existing script: return empty response.
        return ActionsResponseProto.getDefaultInstance();
    }

    /** @see AutofillAssistantService#getNextActions(byte[], byte[], List)  */
    @Override
    public ActionsResponseProto getNextActions(byte[] globalPayload, byte[] scriptPayload,
            List<ProcessedActionProto> processedActions) {
        mProcessedActions = processedActions;
        mNextActionsCounter++;
        ActionsResponseProto responseProto =
                (ActionsResponseProto) ActionsResponseProto.newBuilder()
                        .addAllActions(mNextActions)
                        .setGlobalPayload(ByteString.copyFrom(globalPayload))
                        .setScriptPayload(ByteString.copyFrom(scriptPayload))
                        .build();
        mNextActions = Collections.emptyList();
        return responseProto;
    }

    /** Returns how many times {@code getNextActions} has been called. */
    public int getNextActionsCounter() {
        return mNextActionsCounter;
    }

    /**
     * Synchronously waits until {@code getNextActions} was called at least @{code
     * targetNextActionsCount} times. After this method returns, the list of the most recently
     * returned {@code ProcessedActionProto} can be queried via {@see
     * AutofillAssistantTestService#getProcessedActions}.
     */
    public void waitUntilGetNextActions(int targetNextActionsCount) {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Timeout while waiting for getNextActions") {
                    @Override
                    public boolean isSatisfied() {
                        return mNextActionsCounter >= targetNextActionsCount;
                    }
                });
    }

    /** Access to the most recently received list of processed actions. Is initially null. */
    public @Nullable List<ProcessedActionProto> getProcessedActions() {
        return mProcessedActions;
    }

    @CalledByNative
    private static List<byte[]> createProcessedActionsList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addProcessedAction(List<byte[]> list, byte[] serializedProto) {
        list.add(serializedProto);
    }

    @CalledByNative
    private byte[] getScriptsForUrlNative(String url) {
        return getScriptsForUrl(url).toByteArray();
    }

    @CalledByNative
    private byte[] getActionsNative(
            String scriptPath, String url, byte[] globalPayload, byte[] scriptPayload) {
        return getActions(scriptPath, url, globalPayload, scriptPayload).toByteArray();
    }

    @CalledByNative
    private byte[] getNextActionsNative(
            byte[] globalPayload, byte[] scriptPayload, List<byte[]> processedActions) {
        List<ProcessedActionProto> actions = new ArrayList<>();
        try {
            for (int i = 0; i < processedActions.size(); ++i) {
                actions.add(ProcessedActionProto.parseFrom(processedActions.get(i)));
            }
        } catch (InvalidProtocolBufferException e) {
            e.printStackTrace();
        }
        return getNextActions(globalPayload, scriptPayload, actions).toByteArray();
    }

    @NativeMethods
    interface Natives {
        long javaServiceCreate(AutofillAssistantTestService service);
    }
}
