// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/trees/layer_sorter.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

ScrollAndScaleSet::ScrollAndScaleSet()
{
}

ScrollAndScaleSet::~ScrollAndScaleSet()
{
}

static void sortLayers(std::vector<scoped_refptr<Layer> >::iterator forst, std::vector<scoped_refptr<Layer> >::iterator end, void* layerSorter)
{
    NOTREACHED();
}

static void sortLayers(std::vector<LayerImpl*>::iterator first, std::vector<LayerImpl*>::iterator end, LayerSorter* layerSorter)
{
    DCHECK(layerSorter);
    TRACE_EVENT0("cc", "layer_tree_host_common::sortLayers");
    layerSorter->Sort(first, end);
}

inline gfx::Rect calculateVisibleRectWithCachedLayerRect(const gfx::Rect& targetSurfaceRect, const gfx::Rect& layerBoundRect, const gfx::Rect& layerRectInTargetSpace, const gfx::Transform& transform)
{
    // Is this layer fully contained within the target surface?
    if (targetSurfaceRect.Contains(layerRectInTargetSpace))
        return layerBoundRect;

    // If the layer doesn't fill up the entire surface, then find the part of
    // the surface rect where the layer could be visible. This avoids trying to
    // project surface rect points that are behind the projection point.
    gfx::Rect minimalSurfaceRect = targetSurfaceRect;
    minimalSurfaceRect.Intersect(layerRectInTargetSpace);

    // Project the corners of the target surface rect into the layer space.
    // This bounding rectangle may be larger than it needs to be (being
    // axis-aligned), but is a reasonable filter on the space to consider.
    // Non-invertible transforms will create an empty rect here.

    gfx::Transform surfaceToLayer(gfx::Transform::kSkipInitialization);
    if (!transform.GetInverse(&surfaceToLayer)) {
        // TODO(shawnsingh): Either we need to handle uninvertible transforms
        // here, or DCHECK that the transform is invertible.
    }
    gfx::Rect layerRect = gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(surfaceToLayer, gfx::RectF(minimalSurfaceRect)));
    layerRect.Intersect(layerBoundRect);
    return layerRect;
}

gfx::Rect LayerTreeHostCommon::calculateVisibleRect(const gfx::Rect& targetSurfaceRect, const gfx::Rect& layerBoundRect, const gfx::Transform& transform)
{
    gfx::Rect layerInSurfaceSpace = MathUtil::MapClippedRect(transform, layerBoundRect);
    return calculateVisibleRectWithCachedLayerRect(targetSurfaceRect, layerBoundRect, layerInSurfaceSpace, transform);
}

template <typename LayerType>
static inline bool isRootLayer(LayerType* layer)
{
    return !layer->parent();
}

template<typename LayerType>
static inline bool layerIsInExisting3DRenderingContext(LayerType* layer)
{
    // According to current W3C spec on CSS transforms, a layer is part of an established
    // 3d rendering context if its parent has transform-style of preserves-3d.
    return layer->parent() && layer->parent()->preserves_3d();
}

template<typename LayerType>
static bool isRootLayerOfNewRenderingContext(LayerType* layer)
{
    // According to current W3C spec on CSS transforms (Section 6.1), a layer is the
    // beginning of 3d rendering context if its parent does not have transform-style:
    // preserve-3d, but this layer itself does.
    if (layer->parent())
        return !layer->parent()->preserves_3d() && layer->preserves_3d();

    return layer->preserves_3d();
}

template<typename LayerType>
static bool isLayerBackFaceVisible(LayerType* layer)
{
    // The current W3C spec on CSS transforms says that backface visibility should be
    // determined differently depending on whether the layer is in a "3d rendering
    // context" or not. For Chromium code, we can determine whether we are in a 3d
    // rendering context by checking if the parent preserves 3d.

    if (layerIsInExisting3DRenderingContext(layer))
        return layer->draw_transform().IsBackFaceVisible();

    // In this case, either the layer establishes a new 3d rendering context, or is not in
    // a 3d rendering context at all.
    return layer->transform().IsBackFaceVisible();
}

template<typename LayerType>
static bool isSurfaceBackFaceVisible(LayerType* layer, const gfx::Transform& drawTransform)
{
    if (layerIsInExisting3DRenderingContext(layer))
        return drawTransform.IsBackFaceVisible();

    if (isRootLayerOfNewRenderingContext(layer))
        return layer->transform().IsBackFaceVisible();

    // If the renderSurface is not part of a new or existing rendering context, then the
    // layers that contribute to this surface will decide back-face visibility for themselves.
    return false;
}

template<typename LayerType>
static inline bool layerClipsSubtree(LayerType* layer)
{
    return layer->masks_to_bounds() || layer->mask_layer();
}

template<typename LayerType>
static gfx::Rect calculateVisibleContentRect(LayerType* layer, const gfx::Rect& ancestorClipRectInDescendantSurfaceSpace, const gfx::Rect& layerRectInTargetSpace)
{
    DCHECK(layer->render_target());

    // Nothing is visible if the layer bounds are empty.
    if (!layer->DrawsContent() || layer->content_bounds().IsEmpty() || layer->drawable_content_rect().IsEmpty())
        return gfx::Rect();

    // Compute visible bounds in target surface space.
    gfx::Rect visibleRectInTargetSurfaceSpace = layer->drawable_content_rect();

    if (!layer->render_target()->render_surface()->clip_rect().IsEmpty()) {
        // In this case the target surface does clip layers that contribute to
        // it. So, we have to convert the current surface's clipRect from its
        // ancestor surface space to the current (descendant) surface
        // space. This conversion is done outside this function so that it can
        // be cached instead of computing it redundantly for every layer.
        visibleRectInTargetSurfaceSpace.Intersect(ancestorClipRectInDescendantSurfaceSpace);
    }

    if (visibleRectInTargetSurfaceSpace.IsEmpty())
        return gfx::Rect();

    return calculateVisibleRectWithCachedLayerRect(visibleRectInTargetSurfaceSpace, gfx::Rect(gfx::Point(), layer->content_bounds()), layerRectInTargetSpace, layer->draw_transform());
}

static inline bool transformToParentIsKnown(LayerImpl*)
{
    return true;
}

static inline bool transformToParentIsKnown(Layer* layer)
{

    return !layer->TransformIsAnimating();
}

static inline bool transformToScreenIsKnown(LayerImpl*)
{
    return true;
}

static inline bool transformToScreenIsKnown(Layer* layer)
{
    return !layer->screen_space_transform_is_animating();
}

template<typename LayerType>
static bool layerShouldBeSkipped(LayerType* layer)
{
    // Layers can be skipped if any of these conditions are met.
    //   - does not draw content.
    //   - is transparent
    //   - has empty bounds
    //   - the layer is not double-sided, but its back face is visible.
    //
    // Some additional conditions need to be computed at a later point after the recursion is finished.
    //   - the intersection of render surface content and layer clipRect is empty
    //   - the visibleContentRect is empty
    //
    // Note, if the layer should not have been drawn due to being fully transparent,
    // we would have skipped the entire subtree and never made it into this function,
    // so it is safe to omit this check here.

    if (!layer->DrawsContent() || layer->bounds().IsEmpty())
        return true;

    LayerType* backfaceTestLayer = layer;
    if (layer->use_parent_backface_visibility()) {
        DCHECK(layer->parent());
        DCHECK(!layer->parent()->use_parent_backface_visibility());
        backfaceTestLayer = layer->parent();
    }

    // The layer should not be drawn if (1) it is not double-sided and (2) the back of the layer is known to be facing the screen.
    if (!backfaceTestLayer->double_sided() && transformToScreenIsKnown(backfaceTestLayer) && isLayerBackFaceVisible(backfaceTestLayer))
        return true;

    return false;
}

static inline bool subtreeShouldBeSkipped(LayerImpl* layer)
{
    // The opacity of a layer always applies to its children (either implicitly
    // via a render surface or explicitly if the parent preserves 3D), so the
    // entire subtree can be skipped if this layer is fully transparent.
    return !layer->opacity();
}

static inline bool subtreeShouldBeSkipped(Layer* layer)
{
    // If the opacity is being animated then the opacity on the main thread is unreliable
    // (since the impl thread may be using a different opacity), so it should not be trusted.
    // In particular, it should not cause the subtree to be skipped.
    // Similarly, for layers that might animate opacity using an impl-only
    // animation, their subtree should also not be skipped.
    return !layer->opacity() && !layer->OpacityIsAnimating() &&
           !layer->OpacityCanAnimateOnImplThread();
}

// Called on each layer that could be drawn after all information from
// calcDrawProperties has been updated on that layer.  May have some false
// positives (e.g. layers get this called on them but don't actually get drawn).
static inline void updateTilePrioritiesForLayer(LayerImpl* layer)
{
    layer->UpdateTilePriorities();

    // Mask layers don't get this call, so explicitly update them so they can
    // kick off tile rasterization.
    if (layer->mask_layer())
        layer->mask_layer()->UpdateTilePriorities();
    if (layer->replica_layer() && layer->replica_layer()->mask_layer())
      layer->replica_layer()->mask_layer()->UpdateTilePriorities();
}

static inline void updateTilePrioritiesForLayer(Layer* layer)
{
}

template<typename LayerType>
static bool subtreeShouldRenderToSeparateSurface(LayerType* layer, bool axisAlignedWithRespectToParent)
{
    //
    // A layer and its descendants should render onto a new RenderSurfaceImpl if any of these rules hold:
    //

    // The root layer should always have a renderSurface.
    if (isRootLayer(layer))
        return true;

    // If we force it.
    if (layer->force_render_surface())
        return true;

    // If the layer uses a mask.
    if (layer->mask_layer())
        return true;

    // If the layer has a reflection.
    if (layer->replica_layer())
        return true;

    // If the layer uses a CSS filter.
    if (!layer->filters().isEmpty() || !layer->background_filters().isEmpty() || layer->filter())
        return true;

    int numDescendantsThatDrawContent = layer->draw_properties().num_descendants_that_draw_content;

    // If the layer flattens its subtree (i.e. the layer doesn't preserve-3d), but it is
    // treated as a 3D object by its parent (i.e. parent does preserve-3d).
    if (layerIsInExisting3DRenderingContext(layer) && !layer->preserves_3d() && numDescendantsThatDrawContent > 0) {
        TRACE_EVENT_INSTANT0("cc", "LayerTreeHostCommon::requireSurface flattening");
        return true;
    }

    // If the layer clips its descendants but it is not axis-aligned with respect to its parent.
    bool layerClipsExternalContent = layerClipsSubtree(layer) || layer->HasDelegatedContent();
    if (layerClipsExternalContent && !axisAlignedWithRespectToParent && !layer->draw_properties().descendants_can_clip_selves)
    {
        TRACE_EVENT_INSTANT0("cc", "LayerTreeHostCommon::requireSurface clipping");
        return true;
    }

    // If the layer has some translucency and does not have a preserves-3d transform style.
    // This condition only needs a render surface if two or more layers in the
    // subtree overlap. But checking layer overlaps is unnecessarily costly so
    // instead we conservatively create a surface whenever at least two layers
    // draw content for this subtree.
    bool atLeastTwoLayersInSubtreeDrawContent = numDescendantsThatDrawContent > 0 && (layer->DrawsContent() || numDescendantsThatDrawContent > 1);

    if (layer->opacity() != 1.f && !layer->preserves_3d() && atLeastTwoLayersInSubtreeDrawContent) {
        TRACE_EVENT_INSTANT0("cc", "LayerTreeHostCommon::requireSurface opacity");
        return true;
    }

    return false;
}

gfx::Transform computeScrollCompensationForThisLayer(LayerImpl* scrollingLayer, const gfx::Transform& parentMatrix)
{
    // For every layer that has non-zero scrollDelta, we have to compute a transform that can undo the
    // scrollDelta translation. In particular, we want this matrix to premultiply a fixed-position layer's
    // parentMatrix, so we design this transform in three steps as follows. The steps described here apply
    // from right-to-left, so Step 1 would be the right-most matrix:
    //
    //     Step 1. transform from target surface space to the exact space where scrollDelta is actually applied.
    //           -- this is inverse of the matrix in step 3
    //     Step 2. undo the scrollDelta
    //           -- this is just a translation by scrollDelta.
    //     Step 3. transform back to target surface space.
    //           -- this transform is the "partialLayerOriginTransform" = (parentMatrix * scale(layer->pageScaleDelta()));
    //
    // These steps create a matrix that both start and end in targetSurfaceSpace. So this matrix can
    // pre-multiply any fixed-position layer's drawTransform to undo the scrollDeltas -- as long as
    // that fixed position layer is fixed onto the same renderTarget as this scrollingLayer.
    //

    gfx::Transform partialLayerOriginTransform = parentMatrix;
    partialLayerOriginTransform.PreconcatTransform(scrollingLayer->impl_transform());

    gfx::Transform scrollCompensationForThisLayer = partialLayerOriginTransform; // Step 3
    scrollCompensationForThisLayer.Translate(scrollingLayer->scroll_delta().x(), scrollingLayer->scroll_delta().y()); // Step 2

    gfx::Transform inversePartialLayerOriginTransform(gfx::Transform::kSkipInitialization);
    if (!partialLayerOriginTransform.GetInverse(&inversePartialLayerOriginTransform)) {
        // TODO(shawnsingh): Either we need to handle uninvertible transforms
        // here, or DCHECK that the transform is invertible.
    }
    scrollCompensationForThisLayer.PreconcatTransform(inversePartialLayerOriginTransform); // Step 1
    return scrollCompensationForThisLayer;
}

gfx::Transform computeScrollCompensationMatrixForChildren(Layer* current_layer, const gfx::Transform& currentParentMatrix, const gfx::Transform& currentScrollCompensation)
{
    // The main thread (i.e. Layer) does not need to worry about scroll compensation.
    // So we can just return an identity matrix here.
    return gfx::Transform();
}

gfx::Transform computeScrollCompensationMatrixForChildren(LayerImpl* layer, const gfx::Transform& parentMatrix, const gfx::Transform& currentScrollCompensationMatrix)
{
    // "Total scroll compensation" is the transform needed to cancel out all scrollDelta translations that
    // occurred since the nearest container layer, even if there are renderSurfaces in-between.
    //
    // There are some edge cases to be aware of, that are not explicit in the code:
    //  - A layer that is both a fixed-position and container should not be its own container, instead, that means
    //    it is fixed to an ancestor, and is a container for any fixed-position descendants.
    //  - A layer that is a fixed-position container and has a renderSurface should behave the same as a container
    //    without a renderSurface, the renderSurface is irrelevant in that case.
    //  - A layer that does not have an explicit container is simply fixed to the viewport.
    //    (i.e. the root renderSurface.)
    //  - If the fixed-position layer has its own renderSurface, then the renderSurface is
    //    the one who gets fixed.
    //
    // This function needs to be called AFTER layers create their own renderSurfaces.
    //

    // Avoid the overheads (including stack allocation and matrix initialization/copy) if we know that the scroll compensation doesn't need to be reset or adjusted.
    if (!layer->is_container_for_fixed_position_layers() && layer->scroll_delta().IsZero() && !layer->render_surface())
        return currentScrollCompensationMatrix;

    // Start as identity matrix.
    gfx::Transform nextScrollCompensationMatrix;

    // If this layer is not a container, then it inherits the existing scroll compensations.
    if (!layer->is_container_for_fixed_position_layers())
        nextScrollCompensationMatrix = currentScrollCompensationMatrix;

    // If the current layer has a non-zero scrollDelta, then we should compute its local scrollCompensation
    // and accumulate it to the nextScrollCompensationMatrix.
    if (!layer->scroll_delta().IsZero()) {
        gfx::Transform scrollCompensationForThisLayer = computeScrollCompensationForThisLayer(layer, parentMatrix);
        nextScrollCompensationMatrix.PreconcatTransform(scrollCompensationForThisLayer);
    }

    // If the layer created its own renderSurface, we have to adjust nextScrollCompensationMatrix.
    // The adjustment allows us to continue using the scrollCompensation on the next surface.
    //  Step 1 (right-most in the math): transform from the new surface to the original ancestor surface
    //  Step 2: apply the scroll compensation
    //  Step 3: transform back to the new surface.
    if (layer->render_surface() && !nextScrollCompensationMatrix.IsIdentity()) {
        gfx::Transform inverseSurfaceDrawTransform(gfx::Transform::kSkipInitialization);
        if (!layer->render_surface()->draw_transform().GetInverse(&inverseSurfaceDrawTransform)) {
            // TODO(shawnsingh): Either we need to handle uninvertible transforms
            // here, or DCHECK that the transform is invertible.
        }
        nextScrollCompensationMatrix = inverseSurfaceDrawTransform * nextScrollCompensationMatrix * layer->render_surface()->draw_transform();
    }

    return nextScrollCompensationMatrix;
}

template<typename LayerType>
static inline void CalculateContentsScale(LayerType* layer, float contentsScale, bool animating_transform_to_screen)
{
    layer->CalculateContentsScale(
        contentsScale,
        animating_transform_to_screen,
        &layer->draw_properties().contents_scale_x,
        &layer->draw_properties().contents_scale_y,
        &layer->draw_properties().content_bounds);

    LayerType* maskLayer = layer->mask_layer();
    if (maskLayer)
    {
        maskLayer->CalculateContentsScale(
            contentsScale,
            animating_transform_to_screen,
            &maskLayer->draw_properties().contents_scale_x,
            &maskLayer->draw_properties().contents_scale_y,
            &maskLayer->draw_properties().content_bounds);
    }

    LayerType* replicaMaskLayer = layer->replica_layer() ? layer->replica_layer()->mask_layer() : 0;
    if (replicaMaskLayer)
    {
        replicaMaskLayer->CalculateContentsScale(
            contentsScale,
            animating_transform_to_screen,
            &replicaMaskLayer->draw_properties().contents_scale_x,
            &replicaMaskLayer->draw_properties().contents_scale_y,
            &replicaMaskLayer->draw_properties().content_bounds);
    }
}

static inline void updateLayerContentsScale(LayerImpl* layer, const gfx::Transform& combinedTransform, float deviceScaleFactor, float pageScaleFactor, bool animating_transform_to_screen)
{
    gfx::Vector2dF transformScale = MathUtil::ComputeTransform2dScaleComponents(combinedTransform, deviceScaleFactor * pageScaleFactor);
    float contentsScale = std::max(transformScale.x(), transformScale.y());
    CalculateContentsScale(layer, contentsScale, animating_transform_to_screen);
}

static inline void updateLayerContentsScale(Layer* layer, const gfx::Transform& combinedTransform, float deviceScaleFactor, float pageScaleFactor, bool animating_transform_to_screen)
{
    float rasterScale = layer->raster_scale();

    if (layer->automatically_compute_raster_scale()) {
        gfx::Vector2dF transformScale = MathUtil::ComputeTransform2dScaleComponents(combinedTransform, 0.f);
        float combinedScale = std::max(transformScale.x(), transformScale.y());
        float idealRasterScale = combinedScale / deviceScaleFactor;
        if (!layer->bounds_contain_page_scale())
            idealRasterScale /= pageScaleFactor;

        bool needToSetRasterScale = !rasterScale;

        // If we've previously saved a rasterScale but the ideal changes, things are unpredictable and we should just use 1.
        if (rasterScale && rasterScale != 1.f && idealRasterScale != rasterScale) {
            idealRasterScale = 1.f;
            needToSetRasterScale = true;
        }

        if (needToSetRasterScale) {
            bool useAndSaveIdealScale = idealRasterScale >= 1.f && !animating_transform_to_screen;
            if (useAndSaveIdealScale) {
                rasterScale = idealRasterScale;
                layer->SetRasterScale(rasterScale);
            }
        }
    }

    if (!rasterScale)
        rasterScale = 1.f;

    float contentsScale = rasterScale * deviceScaleFactor;
    if (!layer->bounds_contain_page_scale())
        contentsScale *= pageScaleFactor;

    CalculateContentsScale(layer, contentsScale, animating_transform_to_screen);
}

template<typename LayerType, typename LayerList>
static inline void removeSurfaceForEarlyExit(LayerType* layerToRemove, LayerList& renderSurfaceLayerList)
{
    DCHECK(layerToRemove->render_surface());
    // Technically, we know that the layer we want to remove should be
    // at the back of the renderSurfaceLayerList. However, we have had
    // bugs before that added unnecessary layers here
    // (https://bugs.webkit.org/show_bug.cgi?id=74147), but that causes
    // things to crash. So here we proactively remove any additional
    // layers from the end of the list.
    while (renderSurfaceLayerList.back() != layerToRemove) {
        renderSurfaceLayerList.back()->ClearRenderSurface();
        renderSurfaceLayerList.pop_back();
    }
    DCHECK(renderSurfaceLayerList.back() == layerToRemove);
    renderSurfaceLayerList.pop_back();
    layerToRemove->ClearRenderSurface();
}

// Recursively walks the layer tree to compute any information that is needed
// before doing the main recursion.
template<typename LayerType>
static void preCalculateMetaInformation(LayerType* layer)
{
    if (layer->HasDelegatedContent()) {
        // Layers with delegated content need to be treated as if they have as many children as the number
        // of layers they own delegated quads for. Since we don't know this number right now, we choose
        // one that acts like infinity for our purposes.
        layer->draw_properties().num_descendants_that_draw_content = 1000;
        layer->draw_properties().descendants_can_clip_selves = false;
        return;
    }

    int numDescendantsThatDrawContent = 0;
    bool descendantsCanClipSelves = true;
    bool sublayerTransformPreventsClip = !layer->sublayer_transform().IsPositiveScaleOrTranslation();

    for (size_t i = 0; i < layer->children().size(); ++i) {
        LayerType* childLayer = layer->children()[i];
        preCalculateMetaInformation<LayerType>(childLayer);

        numDescendantsThatDrawContent += childLayer->DrawsContent() ? 1 : 0;
        numDescendantsThatDrawContent += childLayer->draw_properties().num_descendants_that_draw_content;

        if ((childLayer->DrawsContent() && !childLayer->CanClipSelf()) ||
            !childLayer->draw_properties().descendants_can_clip_selves ||
            sublayerTransformPreventsClip ||
            !childLayer->transform().IsPositiveScaleOrTranslation())
            descendantsCanClipSelves = false;
    }

    layer->draw_properties().num_descendants_that_draw_content = numDescendantsThatDrawContent;
    layer->draw_properties().descendants_can_clip_selves = descendantsCanClipSelves;
}

static void roundTranslationComponents(gfx::Transform* transform)
{
    transform->matrix().setDouble(0, 3, MathUtil::Round(transform->matrix().getDouble(0, 3)));
    transform->matrix().setDouble(1, 3, MathUtil::Round(transform->matrix().getDouble(1, 3)));
}

// Recursively walks the layer tree starting at the given node and computes all the
// necessary transformations, clipRects, render surfaces, etc.
template<typename LayerType, typename LayerList, typename RenderSurfaceType>
static void calculateDrawPropertiesInternal(LayerType* layer, const gfx::Transform& parentMatrix,
    const gfx::Transform& fullHierarchyMatrix, const gfx::Transform& currentScrollCompensationMatrix,
    const gfx::Rect& clipRectFromAncestor, const gfx::Rect& clipRectFromAncestorInDescendantSpace, bool ancestorClipsSubtree,
    RenderSurfaceType* nearestAncestorThatMovesPixels, LayerList& renderSurfaceLayerList, LayerList& layerList,
    LayerSorter* layerSorter, int maxTextureSize, float deviceScaleFactor, float pageScaleFactor, bool subtreeCanUseLCDText,
    gfx::Rect& drawableContentRectOfSubtree, bool updateTilePriorities)
{
    // This function computes the new matrix transformations recursively for this
    // layer and all its descendants. It also computes the appropriate render surfaces.
    // Some important points to remember:
    //
    // 0. Here, transforms are notated in Matrix x Vector order, and in words we describe what
    //    the transform does from left to right.
    //
    // 1. In our terminology, the "layer origin" refers to the top-left corner of a layer, and the
    //    positive Y-axis points downwards. This interpretation is valid because the orthographic
    //    projection applied at draw time flips the Y axis appropriately.
    //
    // 2. The anchor point, when given as a PointF object, is specified in "unit layer space",
    //    where the bounds of the layer map to [0, 1]. However, as a Transform object,
    //    the transform to the anchor point is specified in "layer space", where the bounds
    //    of the layer map to [bounds.width(), bounds.height()].
    //
    // 3. Definition of various transforms used:
    //        M[parent] is the parent matrix, with respect to the nearest render surface, passed down recursively.
    //        M[root] is the full hierarchy, with respect to the root, passed down recursively.
    //        Tr[origin] is the translation matrix from the parent's origin to this layer's origin.
    //        Tr[origin2anchor] is the translation from the layer's origin to its anchor point
    //        Tr[origin2center] is the translation from the layer's origin to its center
    //        M[layer] is the layer's matrix (applied at the anchor point)
    //        M[sublayer] is the layer's sublayer transform (also applied at the layer's anchor point)
    //        S[layer2content] is the ratio of a layer's ContentBounds() to its Bounds().
    //
    //    Some composite transforms can help in understanding the sequence of transforms:
    //        compositeLayerTransform = Tr[origin2anchor] * M[layer] * Tr[origin2anchor].inverse()
    //        compositeSublayerTransform = Tr[origin2anchor] * M[sublayer] * Tr[origin2anchor].inverse()
    //
    // 4. When a layer (or render surface) is drawn, it is drawn into a "target render surface". Therefore the draw
    //    transform does not necessarily transform from screen space to local layer space. Instead, the draw transform
    //    is the transform between the "target render surface space" and local layer space. Note that render surfaces,
    //    except for the root, also draw themselves into a different target render surface, and so their draw
    //    transform and origin transforms are also described with respect to the target.
    //
    // Using these definitions, then:
    //
    // The draw transform for the layer is:
    //        M[draw] = M[parent] * Tr[origin] * compositeLayerTransform * S[layer2content]
    //                = M[parent] * Tr[layer->Position() + anchor] * M[layer] * Tr[anchor2origin] * S[layer2content]
    //
    //        Interpreting the math left-to-right, this transforms from the layer's render surface to the origin of the layer in content space.
    //
    // The screen space transform is:
    //        M[screenspace] = M[root] * Tr[origin] * compositeLayerTransform * S[layer2content]
    //                       = M[root] * Tr[layer->Position() + anchor] * M[layer] * Tr[anchor2origin] * S[layer2content]
    //
    //        Interpreting the math left-to-right, this transforms from the root render surface's content space to the origin of the layer in content space.
    //
    // The transform hierarchy that is passed on to children (i.e. the child's parentMatrix) is:
    //        M[parent]_for_child = M[parent] * Tr[origin] * compositeLayerTransform * compositeSublayerTransform
    //                            = M[parent] * Tr[layer->Position() + anchor] * M[layer] * Tr[anchor2origin] * compositeSublayerTransform
    //
    //        and a similar matrix for the full hierarchy with respect to the root.
    //
    // Finally, note that the final matrix used by the shader for the layer is P * M[draw] * S . This final product
    // is computed in drawTexturedQuad(), where:
    //        P is the projection matrix
    //        S is the scale adjustment (to scale up a canonical quad to the layer's size)
    //
    // When a render surface has a replica layer, that layer's transform is used to draw a second copy of the surface.
    // gfx::Transforms named here are relative to the surface, unless they specify they are relative to the replica layer.
    //
    // We will denote a scale by device scale S[deviceScale]
    //
    // The render surface draw transform to its target surface origin is:
    //        M[surfaceDraw] = M[owningLayer->Draw]
    //
    // The render surface origin transform to its the root (screen space) origin is:
    //        M[surface2root] =  M[owningLayer->screenspace] * S[deviceScale].inverse()
    //
    // The replica draw transform to its target surface origin is:
    //        M[replicaDraw] = S[deviceScale] * M[surfaceDraw] * Tr[replica->Position() + replica->anchor()] * Tr[replica] * Tr[origin2anchor].inverse() * S[contentsScale].inverse()
    //
    // The replica draw transform to the root (screen space) origin is:
    //        M[replica2root] = M[surface2root] * Tr[replica->Position()] * Tr[replica] * Tr[origin2anchor].inverse()
    //

    // If we early-exit anywhere in this function, the drawableContentRect of this subtree should be considered empty.
    drawableContentRectOfSubtree = gfx::Rect();

    // The root layer cannot skip calcDrawProperties.
    if (!isRootLayer(layer) && subtreeShouldBeSkipped(layer))
        return;

    // As this function proceeds, these are the properties for the current
    // layer that actually get computed. To avoid unnecessary copies
    // (particularly for matrices), we do computations directly on these values
    // when possible.
    DrawProperties<LayerType, RenderSurfaceType>& layerDrawProperties = layer->draw_properties();

    gfx::Rect clipRectForSubtree;
    bool subtreeShouldBeClipped = false;

    // This value is cached on the stack so that we don't have to inverse-project
    // the surface's clipRect redundantly for every layer. This value is the
    // same as the surface's clipRect, except that instead of being described
    // in the target surface space (i.e. the ancestor surface space), it is
    // described in the current surface space.
    gfx::Rect clipRectForSubtreeInDescendantSpace;

    float accumulatedDrawOpacity = layer->opacity();
    bool animatingOpacityToTarget = layer->OpacityIsAnimating();
    bool animatingOpacityToScreen = animatingOpacityToTarget;
    if (layer->parent()) {
        accumulatedDrawOpacity *= layer->parent()->draw_opacity();
        animatingOpacityToTarget |= layer->parent()->draw_opacity_is_animating();
        animatingOpacityToScreen |= layer->parent()->screen_space_opacity_is_animating();
    }

    bool animatingTransformToTarget = layer->TransformIsAnimating();
    bool animating_transform_to_screen = animatingTransformToTarget;
    if (layer->parent()) {
        animatingTransformToTarget |= layer->parent()->draw_transform_is_animating();
        animating_transform_to_screen |= layer->parent()->screen_space_transform_is_animating();
    }

    gfx::Size bounds = layer->bounds();
    gfx::PointF anchorPoint = layer->anchor_point();
    gfx::PointF position = layer->position() - layer->scroll_delta();

    gfx::Transform combinedTransform = parentMatrix;
    if (!layer->transform().IsIdentity()) {
        // LT = Tr[origin] * Tr[origin2anchor]
        combinedTransform.Translate3d(position.x() + anchorPoint.x() * bounds.width(), position.y() + anchorPoint.y() * bounds.height(), layer->anchor_point_z());
        // LT = Tr[origin] * Tr[origin2anchor] * M[layer]
        combinedTransform.PreconcatTransform(layer->transform());
        // LT = Tr[origin] * Tr[origin2anchor] * M[layer] * Tr[anchor2origin]
        combinedTransform.Translate3d(-anchorPoint.x() * bounds.width(), -anchorPoint.y() * bounds.height(), -layer->anchor_point_z());
    } else {
        combinedTransform.Translate(position.x(), position.y());
    }

    // The layer's contentsSize is determined from the combinedTransform, which then informs the
    // layer's drawTransform.
    updateLayerContentsScale(layer, combinedTransform, deviceScaleFactor, pageScaleFactor, animating_transform_to_screen);

    // If there is a transformation from the impl thread then it should be at
    // the start of the combinedTransform, but we don't want it to affect the
    // computation of contentsScale above.
    // Note carefully: this is Concat, not Preconcat (implTransform * combinedTransform).
    combinedTransform.ConcatTransform(layer->impl_transform());

    if (!animatingTransformToTarget && layer->scrollable() && combinedTransform.IsScaleOrTranslation()) {
        // Align the scrollable layer's position to screen space pixels to avoid blurriness.
        // To avoid side-effects, do this only if the transform is simple.
        roundTranslationComponents(&combinedTransform);
    }

    if (layer->fixed_to_container_layer()) {
        // Special case: this layer is a composited fixed-position layer; we need to
        // explicitly compensate for all ancestors' nonzero scrollDeltas to keep this layer
        // fixed correctly.
        // Note carefully: this is Concat, not Preconcat (currentScrollCompensation * combinedTransform).
        combinedTransform.ConcatTransform(currentScrollCompensationMatrix);
    }

    // The drawTransform that gets computed below is effectively the layer's drawTransform, unless
    // the layer itself creates a renderSurface. In that case, the renderSurface re-parents the transforms.
    layerDrawProperties.target_space_transform = combinedTransform;
    // M[draw] = M[parent] * LT * S[layer2content]
    layerDrawProperties.target_space_transform.Scale(1.0 / layer->contents_scale_x(), 1.0 / layer->contents_scale_y());

    // layerScreenSpaceTransform represents the transform between root layer's "screen space" and local content space.
    layerDrawProperties.screen_space_transform = fullHierarchyMatrix;
    if (!layer->preserves_3d())
        layerDrawProperties.screen_space_transform.FlattenTo2d();
    layerDrawProperties.screen_space_transform.PreconcatTransform(layerDrawProperties.target_space_transform);

    // Adjusting text AA method during animation may cause repaints, which in-turn causes jank.
    bool adjustTextAA = !animatingOpacityToScreen && !animating_transform_to_screen;
    // To avoid color fringing, LCD text should only be used on opaque layers with just integral translation.
    bool layerCanUseLCDText = subtreeCanUseLCDText &&
                              (accumulatedDrawOpacity == 1.0) &&
                              layerDrawProperties.target_space_transform.IsIdentityOrIntegerTranslation();

    gfx::RectF contentRect(gfx::PointF(), layer->content_bounds());

    // fullHierarchyMatrix is the matrix that transforms objects between screen space (except projection matrix) and the most recent RenderSurfaceImpl's space.
    // nextHierarchyMatrix will only change if this layer uses a new RenderSurfaceImpl, otherwise remains the same.
    gfx::Transform nextHierarchyMatrix = fullHierarchyMatrix;
    gfx::Transform sublayerMatrix;

    gfx::Vector2dF renderSurfaceSublayerScale = MathUtil::ComputeTransform2dScaleComponents(combinedTransform, deviceScaleFactor * pageScaleFactor);

    if (subtreeShouldRenderToSeparateSurface(layer, combinedTransform.IsScaleOrTranslation())) {
        // Check back-face visibility before continuing with this surface and its subtree
        if (!layer->double_sided() && transformToParentIsKnown(layer) && isSurfaceBackFaceVisible(layer, combinedTransform))
            return;

        if (!layer->render_surface())
            layer->CreateRenderSurface();

        RenderSurfaceType* renderSurface = layer->render_surface();
        renderSurface->ClearLayerLists();

        // The owning layer's draw transform has a scale from content to layer
        // space which we do not want; so here we use the combinedTransform
        // instead of the drawTransform. However, we do need to add a different
        // scale factor that accounts for the surface's pixel dimensions.
        combinedTransform.Scale(1 / renderSurfaceSublayerScale.x(), 1 / renderSurfaceSublayerScale.y());
        renderSurface->SetDrawTransform(combinedTransform);

        // The owning layer's transform was re-parented by the surface, so the layer's new drawTransform
        // only needs to scale the layer to surface space.
        layerDrawProperties.target_space_transform.MakeIdentity();
        layerDrawProperties.target_space_transform.Scale(renderSurfaceSublayerScale.x() / layer->contents_scale_x(), renderSurfaceSublayerScale.y() / layer->contents_scale_y());

        // Inside the surface's subtree, we scale everything to the owning layer's scale.
        // The sublayer matrix transforms layer rects into target
        // surface content space.
        DCHECK(sublayerMatrix.IsIdentity());
        sublayerMatrix.Scale(renderSurfaceSublayerScale.x(), renderSurfaceSublayerScale.y());

        // The opacity value is moved from the layer to its surface, so that the entire subtree properly inherits opacity.
        renderSurface->SetDrawOpacity(accumulatedDrawOpacity);
        renderSurface->SetDrawOpacityIsAnimating(animatingOpacityToTarget);
        animatingOpacityToTarget = false;
        layerDrawProperties.opacity = 1;
        layerDrawProperties.opacity_is_animating = animatingOpacityToTarget;
        layerDrawProperties.screen_space_opacity_is_animating = animatingOpacityToScreen;

        renderSurface->SetTargetSurfaceTransformsAreAnimating(animatingTransformToTarget);
        renderSurface->SetScreenSpaceTransformsAreAnimating(animating_transform_to_screen);
        animatingTransformToTarget = false;
        layerDrawProperties.target_space_transform_is_animating = animatingTransformToTarget;
        layerDrawProperties.screen_space_transform_is_animating = animating_transform_to_screen;

        // Update the aggregate hierarchy matrix to include the transform of the
        // newly created RenderSurfaceImpl.
        nextHierarchyMatrix.PreconcatTransform(renderSurface->draw_transform());

        // The new renderSurface here will correctly clip the entire subtree. So, we do
        // not need to continue propagating the clipping state further down the tree. This
        // way, we can avoid transforming clipRects from ancestor target surface space to
        // current target surface space that could cause more w < 0 headaches.
        subtreeShouldBeClipped = false;

        if (layer->mask_layer()) {
            DrawProperties<LayerType, RenderSurfaceType>& maskLayerDrawProperties = layer->mask_layer()->draw_properties();
            maskLayerDrawProperties.render_target = layer;
            maskLayerDrawProperties.visible_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
        }

        if (layer->replica_layer() && layer->replica_layer()->mask_layer()) {
            DrawProperties<LayerType, RenderSurfaceType>& replicaMaskDrawProperties = layer->replica_layer()->mask_layer()->draw_properties();
            replicaMaskDrawProperties.render_target = layer;
            replicaMaskDrawProperties.visible_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
        }

        // FIXME:  make this smarter for the SkImageFilter case (check for
        //         pixel-moving filters)
        if (layer->filters().hasFilterThatMovesPixels() || layer->filter())
            nearestAncestorThatMovesPixels = renderSurface;

        // The render surface clipRect is expressed in the space where this surface draws, i.e. the same space as clipRectFromAncestor.
        renderSurface->SetIsClipped(ancestorClipsSubtree);
        if (ancestorClipsSubtree) {
            renderSurface->SetClipRect(clipRectFromAncestor);

            gfx::Transform inverseSurfaceDrawTransform(gfx::Transform::kSkipInitialization);
            if (!renderSurface->draw_transform().GetInverse(&inverseSurfaceDrawTransform)) {
                // TODO(shawnsingh): Either we need to handle uninvertible transforms
                // here, or DCHECK that the transform is invertible.
            }
            clipRectForSubtreeInDescendantSpace = gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(inverseSurfaceDrawTransform, renderSurface->clip_rect()));
        } else {
            renderSurface->SetClipRect(gfx::Rect());
            clipRectForSubtreeInDescendantSpace = clipRectFromAncestorInDescendantSpace;
        }

        renderSurface->SetNearestAncestorThatMovesPixels(nearestAncestorThatMovesPixels);

        // If the new render surface is drawn translucent or with a non-integral translation
        // then the subtree that gets drawn on this render surface cannot use LCD text.
        subtreeCanUseLCDText = layerCanUseLCDText;

        renderSurfaceLayerList.push_back(layer);
    } else {
        DCHECK(layer->parent());

        // Note: layerDrawProperties.target_space_transform is computed above,
        // before this if-else statement.
        layerDrawProperties.target_space_transform_is_animating = animatingTransformToTarget;
        layerDrawProperties.screen_space_transform_is_animating = animating_transform_to_screen;
        layerDrawProperties.opacity = accumulatedDrawOpacity;
        layerDrawProperties.opacity_is_animating = animatingOpacityToTarget;
        layerDrawProperties.screen_space_opacity_is_animating = animatingOpacityToScreen;
        sublayerMatrix = combinedTransform;

        layer->ClearRenderSurface();

        // Layers without renderSurfaces directly inherit the ancestor's clip status.
        subtreeShouldBeClipped = ancestorClipsSubtree;
        if (ancestorClipsSubtree)
            clipRectForSubtree = clipRectFromAncestor;

        // The surface's cached clipRect value propagates regardless of what clipping goes on between layers here.
        clipRectForSubtreeInDescendantSpace = clipRectFromAncestorInDescendantSpace;

        // Layers that are not their own renderTarget will render into the target of their nearest ancestor.
        layerDrawProperties.render_target = layer->parent()->render_target();
    }

    if (adjustTextAA)
        layerDrawProperties.can_use_lcd_text = layerCanUseLCDText;

    gfx::Rect rectInTargetSpace = ToEnclosingRect(MathUtil::MapClippedRect(layer->draw_transform(), contentRect));

    if (layerClipsSubtree(layer)) {
        subtreeShouldBeClipped = true;
        if (ancestorClipsSubtree && !layer->render_surface()) {
            clipRectForSubtree = clipRectFromAncestor;
            clipRectForSubtree.Intersect(rectInTargetSpace);
        } else
            clipRectForSubtree = rectInTargetSpace;
    }

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!layer->preserves_3d())
        sublayerMatrix.FlattenTo2d();

    // Apply the sublayer transform at the anchor point of the layer.
    if (!layer->sublayer_transform().IsIdentity()) {
        sublayerMatrix.Translate(layer->anchor_point().x() * bounds.width(), layer->anchor_point().y() * bounds.height());
        sublayerMatrix.PreconcatTransform(layer->sublayer_transform());
        sublayerMatrix.Translate(-layer->anchor_point().x() * bounds.width(), -layer->anchor_point().y() * bounds.height());
    }

    LayerList& descendants = (layer->render_surface() ? layer->render_surface()->layer_list() : layerList);

    // Any layers that are appended after this point are in the layer's subtree and should be included in the sorting process.
    unsigned sortingStartIndex = descendants.size();

    if (!layerShouldBeSkipped(layer))
        descendants.push_back(layer);

    gfx::Transform nextScrollCompensationMatrix = computeScrollCompensationMatrixForChildren(layer, parentMatrix, currentScrollCompensationMatrix);;

    gfx::Rect accumulatedDrawableContentRectOfChildren;
    for (size_t i = 0; i < layer->children().size(); ++i) {
        LayerType* child = LayerTreeHostCommon::getChildAsRawPtr(layer->children(), i);
        gfx::Rect drawableContentRectOfChildSubtree;
        calculateDrawPropertiesInternal<LayerType, LayerList, RenderSurfaceType>(child, sublayerMatrix, nextHierarchyMatrix, nextScrollCompensationMatrix,
                                                                                 clipRectForSubtree, clipRectForSubtreeInDescendantSpace, subtreeShouldBeClipped, nearestAncestorThatMovesPixels,
                                                                                 renderSurfaceLayerList, descendants, layerSorter, maxTextureSize, deviceScaleFactor, pageScaleFactor,
                                                                                 subtreeCanUseLCDText, drawableContentRectOfChildSubtree, updateTilePriorities);
        if (!drawableContentRectOfChildSubtree.IsEmpty()) {
            accumulatedDrawableContentRectOfChildren.Union(drawableContentRectOfChildSubtree);
            if (child->render_surface())
                descendants.push_back(child);
        }
    }

    if (layer->render_surface() && !isRootLayer(layer) && !layer->render_surface()->layer_list().size()) {
        removeSurfaceForEarlyExit(layer, renderSurfaceLayerList);
        return;
    }
    
    // Compute the total drawableContentRect for this subtree (the rect is in targetSurface space)
    gfx::Rect localDrawableContentRectOfSubtree = accumulatedDrawableContentRectOfChildren;
    if (layer->DrawsContent())
        localDrawableContentRectOfSubtree.Union(rectInTargetSpace);
    if (subtreeShouldBeClipped)
        localDrawableContentRectOfSubtree.Intersect(clipRectForSubtree);

    // Compute the layer's drawable content rect (the rect is in targetSurface space)
    layerDrawProperties.drawable_content_rect = rectInTargetSpace;
    if (subtreeShouldBeClipped)
        layerDrawProperties.drawable_content_rect.Intersect(clipRectForSubtree);

    // Tell the layer the rect that is clipped by. In theory we could use a
    // tighter clipRect here (drawableContentRect), but that actually does not
    // reduce how much would be drawn, and instead it would create unnecessary
    // changes to scissor state affecting GPU performance.
    layerDrawProperties.is_clipped = subtreeShouldBeClipped;
    if (subtreeShouldBeClipped)
        layerDrawProperties.clip_rect = clipRectForSubtree;
    else {
        // Initialize the clipRect to a safe value that will not clip the
        // layer, just in case clipping is still accidentally used.
        layerDrawProperties.clip_rect = rectInTargetSpace;
    }

    // Compute the layer's visible content rect (the rect is in content space)
    layerDrawProperties.visible_content_rect = calculateVisibleContentRect(layer, clipRectForSubtreeInDescendantSpace, rectInTargetSpace);

    // Compute the remaining properties for the render surface, if the layer has one.
    if (isRootLayer(layer)) {
        // The root layer's surface's contentRect is always the entire viewport.
        DCHECK(layer->render_surface());
        layer->render_surface()->SetContentRect(clipRectFromAncestor);
    } else if (layer->render_surface() && !isRootLayer(layer)) {
        RenderSurfaceType* renderSurface = layer->render_surface();
        gfx::Rect clippedContentRect = localDrawableContentRectOfSubtree;

        // Don't clip if the layer is reflected as the reflection shouldn't be
        // clipped. If the layer is animating, then the surface's transform to
        // its target is not known on the main thread, and we should not use it
        // to clip.
        if (!layer->replica_layer() && transformToParentIsKnown(layer)) {
            // Note, it is correct to use ancestorClipsSubtree here, because we are looking at this layer's renderSurface, not the layer itself.
            if (ancestorClipsSubtree && !clippedContentRect.IsEmpty()) {
                gfx::Rect surfaceClipRect = LayerTreeHostCommon::calculateVisibleRect(renderSurface->clip_rect(), clippedContentRect, renderSurface->draw_transform());
                clippedContentRect.Intersect(surfaceClipRect);
            }
        }

        // The RenderSurfaceImpl backing texture cannot exceed the maximum supported
        // texture size.
        clippedContentRect.set_width(std::min(clippedContentRect.width(), maxTextureSize));
        clippedContentRect.set_height(std::min(clippedContentRect.height(), maxTextureSize));

        if (clippedContentRect.IsEmpty()) {
            renderSurface->ClearLayerLists();
            removeSurfaceForEarlyExit(layer, renderSurfaceLayerList);
            return;
        }
        
        renderSurface->SetContentRect(clippedContentRect);

        // The owning layer's screenSpaceTransform has a scale from content to layer space which we need to undo and
        // replace with a scale from the surface's subtree into layer space.
        gfx::Transform screenSpaceTransform = layer->screen_space_transform();
        screenSpaceTransform.Scale(layer->contents_scale_x() / renderSurfaceSublayerScale.x(), layer->contents_scale_y() / renderSurfaceSublayerScale.y());
        renderSurface->SetScreenSpaceTransform(screenSpaceTransform);

        if (layer->replica_layer()) {
            gfx::Transform surfaceOriginToReplicaOriginTransform;
            surfaceOriginToReplicaOriginTransform.Scale(renderSurfaceSublayerScale.x(), renderSurfaceSublayerScale.y());
            surfaceOriginToReplicaOriginTransform.Translate(layer->replica_layer()->position().x() + layer->replica_layer()->anchor_point().x() * bounds.width(),
                                                            layer->replica_layer()->position().y() + layer->replica_layer()->anchor_point().y() * bounds.height());
            surfaceOriginToReplicaOriginTransform.PreconcatTransform(layer->replica_layer()->transform());
            surfaceOriginToReplicaOriginTransform.Translate(-layer->replica_layer()->anchor_point().x() * bounds.width(), -layer->replica_layer()->anchor_point().y() * bounds.height());
            surfaceOriginToReplicaOriginTransform.Scale(1 / renderSurfaceSublayerScale.x(), 1 / renderSurfaceSublayerScale.y());

            // Compute the replica's "originTransform" that maps from the replica's origin space to the target surface origin space.
            gfx::Transform replicaOriginTransform = layer->render_surface()->draw_transform() * surfaceOriginToReplicaOriginTransform;
            renderSurface->SetReplicaDrawTransform(replicaOriginTransform);

            // Compute the replica's "screenSpaceTransform" that maps from the replica's origin space to the screen's origin space.
            gfx::Transform replicaScreenSpaceTransform = layer->render_surface()->screen_space_transform() * surfaceOriginToReplicaOriginTransform;
            renderSurface->SetReplicaScreenSpaceTransform(replicaScreenSpaceTransform);
        }
    }

    if (updateTilePriorities)
        updateTilePrioritiesForLayer(layer);

    // If neither this layer nor any of its children were added, early out.
    if (sortingStartIndex == descendants.size())
        return;

    // If preserves-3d then sort all the descendants in 3D so that they can be
    // drawn from back to front. If the preserves-3d property is also set on the parent then
    // skip the sorting as the parent will sort all the descendants anyway.
    if (layerSorter && descendants.size() && layer->preserves_3d() && (!layer->parent() || !layer->parent()->preserves_3d()))
        sortLayers(descendants.begin() + sortingStartIndex, descendants.end(), layerSorter);

    if (layer->render_surface())
        drawableContentRectOfSubtree = gfx::ToEnclosingRect(layer->render_surface()->DrawableContentRect());
    else
        drawableContentRectOfSubtree = localDrawableContentRectOfSubtree;

    if (layer->HasContributingDelegatedRenderPasses())
        layer->render_target()->render_surface()->AddContributingDelegatedRenderPassLayer(layer);
}

void LayerTreeHostCommon::calculateDrawProperties(Layer* rootLayer, const gfx::Size& deviceViewportSize, float deviceScaleFactor, float pageScaleFactor, int maxTextureSize, bool canUseLCDText, std::vector<scoped_refptr<Layer> >& renderSurfaceLayerList)
{
    gfx::Rect totalDrawableContentRect;
    gfx::Transform identityMatrix;
    gfx::Transform deviceScaleTransform;
    deviceScaleTransform.Scale(deviceScaleFactor, deviceScaleFactor);
    std::vector<scoped_refptr<Layer> > dummyLayerList;

    // The root layer's renderSurface should receive the deviceViewport as the initial clipRect.
    bool subtreeShouldBeClipped = true;
    gfx::Rect deviceViewportRect(gfx::Point(), deviceViewportSize);
    bool updateTilePriorities = false;

    // This function should have received a root layer.
    DCHECK(isRootLayer(rootLayer));

    preCalculateMetaInformation<Layer>(rootLayer);
    calculateDrawPropertiesInternal<Layer, std::vector<scoped_refptr<Layer> >, RenderSurface>(
        rootLayer, deviceScaleTransform, identityMatrix, identityMatrix,
        deviceViewportRect, deviceViewportRect, subtreeShouldBeClipped, 0, renderSurfaceLayerList,
        dummyLayerList, 0, maxTextureSize,
        deviceScaleFactor, pageScaleFactor, canUseLCDText, totalDrawableContentRect,
        updateTilePriorities);

    // The dummy layer list should not have been used.
    DCHECK(dummyLayerList.size() == 0);
    // A root layer renderSurface should always exist after calculateDrawProperties.
    DCHECK(rootLayer->render_surface());
}

void LayerTreeHostCommon::calculateDrawProperties(LayerImpl* rootLayer, const gfx::Size& deviceViewportSize, float deviceScaleFactor, float pageScaleFactor, int maxTextureSize, bool canUseLCDText, std::vector<LayerImpl*>& renderSurfaceLayerList, bool updateTilePriorities)
{
    gfx::Rect totalDrawableContentRect;
    gfx::Transform identityMatrix;
    gfx::Transform deviceScaleTransform;
    deviceScaleTransform.Scale(deviceScaleFactor, deviceScaleFactor);
    std::vector<LayerImpl*> dummyLayerList;
    LayerSorter layerSorter;
    
    // The root layer's renderSurface should receive the deviceViewport as the initial clipRect.
    bool subtreeShouldBeClipped = true;
    gfx::Rect deviceViewportRect(gfx::Point(), deviceViewportSize);

    // This function should have received a root layer.
    DCHECK(isRootLayer(rootLayer));

    preCalculateMetaInformation<LayerImpl>(rootLayer);
    calculateDrawPropertiesInternal<LayerImpl, std::vector<LayerImpl*>, RenderSurfaceImpl>(
        rootLayer, deviceScaleTransform, identityMatrix, identityMatrix,
        deviceViewportRect, deviceViewportRect, subtreeShouldBeClipped, 0, renderSurfaceLayerList,
        dummyLayerList, &layerSorter, maxTextureSize,
        deviceScaleFactor, pageScaleFactor, canUseLCDText, totalDrawableContentRect,
        updateTilePriorities);

    // The dummy layer list should not have been used.
    DCHECK(dummyLayerList.size() == 0);
    // A root layer renderSurface should always exist after calculateDrawProperties.
    DCHECK(rootLayer->render_surface());
}

static bool pointHitsRect(const gfx::PointF& screenSpacePoint, const gfx::Transform& localSpaceToScreenSpaceTransform, gfx::RectF localSpaceRect)
{
    // If the transform is not invertible, then assume that this point doesn't hit this rect.
    gfx::Transform inverseLocalSpaceToScreenSpace(gfx::Transform::kSkipInitialization);
    if (!localSpaceToScreenSpaceTransform.GetInverse(&inverseLocalSpaceToScreenSpace))
        return false;

    // Transform the hit test point from screen space to the local space of the given rect.
    bool clipped = false;
    gfx::PointF hitTestPointInLocalSpace = MathUtil::ProjectPoint(inverseLocalSpaceToScreenSpace, screenSpacePoint, &clipped);

    // If projectPoint could not project to a valid value, then we assume that this point doesn't hit this rect.
    if (clipped)
        return false;

    return localSpaceRect.Contains(hitTestPointInLocalSpace);
}

static bool pointHitsRegion(gfx::PointF screenSpacePoint, const gfx::Transform& screenSpaceTransform, const Region& layerSpaceRegion, float layerContentScaleX, float layerContentScaleY)
{
    // If the transform is not invertible, then assume that this point doesn't hit this region.
    gfx::Transform inverseScreenSpaceTransform(gfx::Transform::kSkipInitialization);
    if (!screenSpaceTransform.GetInverse(&inverseScreenSpaceTransform))
        return false;

    // Transform the hit test point from screen space to the local space of the given region.
    bool clipped = false;
    gfx::PointF hitTestPointInContentSpace = MathUtil::ProjectPoint(inverseScreenSpaceTransform, screenSpacePoint, &clipped);
    gfx::PointF hitTestPointInLayerSpace = gfx::ScalePoint(hitTestPointInContentSpace, 1 / layerContentScaleX, 1 / layerContentScaleY);

    // If projectPoint could not project to a valid value, then we assume that this point doesn't hit this region.
    if (clipped)
        return false;

    return layerSpaceRegion.Contains(gfx::ToRoundedPoint(hitTestPointInLayerSpace));
}

static bool pointIsClippedBySurfaceOrClipRect(const gfx::PointF& screenSpacePoint, LayerImpl* layer)
{
    LayerImpl* current_layer = layer;

    // Walk up the layer tree and hit-test any renderSurfaces and any layer clipRects that are active.
    while (current_layer) {
        if (current_layer->render_surface() && !pointHitsRect(screenSpacePoint, current_layer->render_surface()->screen_space_transform(), current_layer->render_surface()->content_rect()))
            return true;

        // Note that drawableContentRects are actually in targetSurface space, so the transform we
        // have to provide is the target surface's screenSpaceTransform.
        LayerImpl* renderTarget = current_layer->render_target();
        if (layerClipsSubtree(current_layer) && !pointHitsRect(screenSpacePoint, renderTarget->render_surface()->screen_space_transform(), current_layer->drawable_content_rect()))
            return true;

        current_layer = current_layer->parent();
    }

    // If we have finished walking all ancestors without having already exited, then the point is not clipped by any ancestors.
    return false;
}

LayerImpl* LayerTreeHostCommon::findLayerThatIsHitByPoint(const gfx::PointF& screenSpacePoint, const std::vector<LayerImpl*>& renderSurfaceLayerList)
{
    LayerImpl* foundLayer = 0;

    typedef LayerIterator<LayerImpl, std::vector<LayerImpl*>, RenderSurfaceImpl, LayerIteratorActions::FrontToBack> LayerIteratorType;
    LayerIteratorType end = LayerIteratorType::End(&renderSurfaceLayerList);

    for (LayerIteratorType it = LayerIteratorType::Begin(&renderSurfaceLayerList); it != end; ++it) {
        // We don't want to consider renderSurfaces for hit testing.
        if (!it.represents_itself())
            continue;

        LayerImpl* current_layer = (*it);

        gfx::RectF contentRect(gfx::PointF(), current_layer->content_bounds());
        if (!pointHitsRect(screenSpacePoint, current_layer->screen_space_transform(), contentRect))
            continue;

        // At this point, we think the point does hit the layer, but we need to walk up
        // the parents to ensure that the layer was not clipped in such a way that the
        // hit point actually should not hit the layer.
        if (pointIsClippedBySurfaceOrClipRect(screenSpacePoint, current_layer))
            continue;

        // Skip the HUD layer.
        if (current_layer == current_layer->layer_tree_impl()->hud_layer())
            continue;

        foundLayer = current_layer;
        break;
    }

    // This can potentially return 0, which means the screenSpacePoint did not successfully hit test any layers, not even the root layer.
    return foundLayer;
}

LayerImpl* LayerTreeHostCommon::findLayerThatIsHitByPointInTouchHandlerRegion(const gfx::PointF& screenSpacePoint, const std::vector<LayerImpl*>& renderSurfaceLayerList)
{
    LayerImpl* foundLayer = 0;

    typedef LayerIterator<LayerImpl, std::vector<LayerImpl*>, RenderSurfaceImpl, LayerIteratorActions::FrontToBack> LayerIteratorType;
    LayerIteratorType end = LayerIteratorType::End(&renderSurfaceLayerList);

    for (LayerIteratorType it = LayerIteratorType::Begin(&renderSurfaceLayerList); it != end; ++it) {
        // We don't want to consider renderSurfaces for hit testing.
        if (!it.represents_itself())
            continue;

        LayerImpl* current_layer = (*it);

        if (!layerHasTouchEventHandlersAt(screenSpacePoint, current_layer))
            continue;

        foundLayer = current_layer;
        break;
    }

    // This can potentially return 0, which means the screenSpacePoint did not successfully hit test any layers, not even the root layer.
    return foundLayer;
}

bool LayerTreeHostCommon::layerHasTouchEventHandlersAt(const gfx::PointF& screenSpacePoint, LayerImpl* layerImpl) {
  if (layerImpl->touch_event_handler_region().IsEmpty())
      return false;

  if (!pointHitsRegion(screenSpacePoint, layerImpl->screen_space_transform(), layerImpl->touch_event_handler_region(), layerImpl->contents_scale_x(), layerImpl->contents_scale_y()))
     return false;;

  // At this point, we think the point does hit the touch event handler region on the layer, but we need to walk up
  // the parents to ensure that the layer was not clipped in such a way that the
  // hit point actually should not hit the layer.
  if (pointIsClippedBySurfaceOrClipRect(screenSpacePoint, layerImpl))
     return false;

  return true;
}
}  // namespace cc
