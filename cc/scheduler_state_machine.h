// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_STATE_MACHINE_H_
#define CC_SCHEDULER_STATE_MACHINE_H_

#include <string>

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/scheduler_settings.h"

namespace cc {

// The SchedulerStateMachine decides how to coordinate main thread activites
// like painting/running javascript with rendering and input activities on the
// impl thread.
//
// The state machine tracks internal state but is also influenced by external state.
// Internal state includes things like whether a frame has been requested, while
// external state includes things like the current time being near to the vblank time.
//
// The scheduler seperates "what to do next" from the updating of its internal state to
// make testing cleaner.
class CC_EXPORT SchedulerStateMachine {
public:
    // settings must be valid for the lifetime of this class.
    SchedulerStateMachine(const SchedulerSettings& settings);

    enum CommitState {
        COMMIT_STATE_IDLE,
        COMMIT_STATE_FRAME_IN_PROGRESS,
        COMMIT_STATE_READY_TO_COMMIT,
        COMMIT_STATE_WAITING_FOR_FIRST_DRAW,
        COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW,
   };

    enum TextureState {
        LAYER_TEXTURE_STATE_UNLOCKED,
        LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD,
        LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD,
    };

    enum OutputSurfaceState {
        OUTPUT_SURFACE_ACTIVE,
        OUTPUT_SURFACE_LOST,
        OUTPUT_SURFACE_RECREATING,
    };

    bool commitPending() const
    {
        return m_commitState == COMMIT_STATE_FRAME_IN_PROGRESS ||
            m_commitState == COMMIT_STATE_READY_TO_COMMIT;
    }

    bool redrawPending() const { return m_needsRedraw; }

    enum Action {
        ACTION_NONE,
        ACTION_BEGIN_FRAME,
        ACTION_COMMIT,
        ACTION_CHECK_FOR_COMPLETED_TILE_UPLOADS,
        ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED,
        ACTION_DRAW_IF_POSSIBLE,
        ACTION_DRAW_FORCED,
        ACTION_BEGIN_OUTPUT_SURFACE_RECREATION,
        ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD,
    };
    Action nextAction() const;
    void updateState(Action);

    // Indicates whether the scheduler needs a vsync callback in order to make
    // progress.
    bool vsyncCallbackNeeded() const;

    // Indicates that the system has entered and left a vsync callback.
    // The scheduler will not draw more than once in a given vsync callback.
    void didEnterVSync();
    void didLeaveVSync();

    // Indicates whether the LayerTreeHostImpl is visible.
    void setVisible(bool);

    // Indicates that a redraw is required, either due to the impl tree changing
    // or the screen being damaged and simply needing redisplay.
    void setNeedsRedraw();

    // As setNeedsRedraw(), but ensures the draw will definitely happen even if
    // we are not visible.
    void setNeedsForcedRedraw();

    // Indicates that a redraw is required because we are currently rendering
    // with a low resolution or checkerboarded tile.
    void didSwapUseIncompleteTile();

    // Indicates whether ACTION_DRAW_IF_POSSIBLE drew to the screen or not.
    void didDrawIfPossibleCompleted(bool success);

    // Indicates that a new commit flow needs to be performed, either to pull
    // updates from the main thread to the impl, or to push deltas from the impl
    // thread to main.
    void setNeedsCommit();

    // As setNeedsCommit(), but ensures the beginFrame will definitely happen even if
    // we are not visible.
    // After this call we expect to go through the forced commit flow and then return
    // to waiting for a non-forced beginFrame to finish.
    void setNeedsForcedCommit();

    // Call this only in response to receiving an ACTION_BEGIN_FRAME
    // from nextState. Indicates that all painting is complete.
    void beginFrameComplete();

    // Call this only in response to receiving an ACTION_BEGIN_FRAME
    // from nextState if the client rejects the beginFrame message.
    void beginFrameAborted();

    // Request exclusive access to the textures that back single buffered
    // layers on behalf of the main thread. Upon acqusition,
    // ACTION_DRAW_IF_POSSIBLE will not draw until the main thread releases the
    // textures to the impl thread by committing the layers.
    void setMainThreadNeedsLayerTextures();

    // Indicates whether we can successfully begin a frame at this time.
    void setCanBeginFrame(bool can) { m_canBeginFrame = can; }

    // Indicates whether drawing would, at this time, make sense.
    // canDraw can be used to supress flashes or checkerboarding
    // when such behavior would be undesirable.
    void setCanDraw(bool can);

    // Indicates whether or not there is a pending tree.  This influences
    // whether or not we can succesfully commit at this time.  If the
    // last commit is still being processed (but not blocking), it may not
    // be possible to take another commit yet.  This overrides force commit,
    // as a commit is already still in flight.
    void setHasPendingTree(bool hasPendingTree);
    bool hasPendingTree() const { return m_hasPendingTree; }

    void didLoseOutputSurface();
    void didRecreateOutputSurface();

    // Exposed for testing purposes.
    void setMaximumNumberOfFailedDrawsBeforeDrawIsForced(int);

    std::string toString();

protected:
    bool shouldDrawForced() const;
    bool drawSuspendedUntilCommit() const;
    bool scheduledToDraw() const;
    bool shouldDraw() const;
    bool shouldAttemptTreeActivation() const;
    bool shouldAcquireLayerTexturesForMainThread() const;
    bool shouldCheckForCompletedTileUploads() const;
    bool hasDrawnThisFrame() const;
    bool hasAttemptedTreeActivationThisFrame() const;
    bool hasCheckedForCompletedTileUploadsThisFrame() const;

    const SchedulerSettings m_settings;

    CommitState m_commitState;

    int m_currentFrameNumber;
    int m_lastFrameNumberWhereDrawWasCalled;
    int m_lastFrameNumberWhereTreeActivationAttempted;
    int m_lastFrameNumberWhereCheckForCompletedTileUploadsCalled;
    int m_consecutiveFailedDraws;
    int m_maximumNumberOfFailedDrawsBeforeDrawIsForced;
    bool m_needsRedraw;
    bool m_swapUsedIncompleteTile;
    bool m_needsForcedRedraw;
    bool m_needsForcedRedrawAfterNextCommit;
    bool m_needsCommit;
    bool m_needsForcedCommit;
    bool m_expectImmediateBeginFrame;
    bool m_mainThreadNeedsLayerTextures;
    bool m_insideVSync;
    bool m_visible;
    bool m_canBeginFrame;
    bool m_canDraw;
    bool m_hasPendingTree;
    bool m_drawIfPossibleFailed;
    TextureState m_textureState;
    OutputSurfaceState m_outputSurfaceState;

    DISALLOW_COPY_AND_ASSIGN(SchedulerStateMachine);
};

}

#endif  // CC_SCHEDULER_STATE_MACHINE_H_
