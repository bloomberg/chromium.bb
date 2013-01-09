// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_impl.h"

#include "base/debug/trace_event.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "cc/animation_registrar.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/debug_colors.h"
#include "cc/layer_tree_debug_state.h"
#include "cc/layer_tree_impl.h"
#include "cc/layer_tree_settings.h"
#include "cc/math_util.h"
#include "cc/proxy.h"
#include "cc/quad_sink.h"
#include "cc/scrollbar_animation_controller.h"
#include "cc/scrollbar_animation_controller_linear_fade.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

LayerImpl::LayerImpl(LayerTreeImpl* treeImpl, int id)
    : m_parent(0)
    , m_maskLayerId(-1)
    , m_replicaLayerId(-1)
    , m_layerId(id)
    , m_layerTreeImpl(treeImpl)
    , m_anchorPoint(0.5, 0.5)
    , m_anchorPointZ(0)
    , m_scrollable(false)
    , m_shouldScrollOnMainThread(false)
    , m_haveWheelEventHandlers(false)
    , m_backgroundColor(0)
    , m_doubleSided(true)
    , m_layerPropertyChanged(false)
    , m_layerSurfacePropertyChanged(false)
    , m_masksToBounds(false)
    , m_contentsOpaque(false)
    , m_opacity(1.0)
    , m_preserves3D(false)
    , m_useParentBackfaceVisibility(false)
    , m_drawCheckerboardForMissingTiles(false)
    , m_drawsContent(false)
    , m_forceRenderSurface(false)
    , m_isContainerForFixedPositionLayers(false)
    , m_fixedToContainerLayer(false)
    , m_drawDepth(0)
#ifndef NDEBUG
    , m_betweenWillDrawAndDidDraw(false)
#endif
{
    DCHECK(m_layerId > 0);
    DCHECK(m_layerTreeImpl);
    m_layerTreeImpl->RegisterLayer(this);
    AnimationRegistrar* registrar = m_layerTreeImpl->animationRegistrar();
    m_layerAnimationController = registrar->GetAnimationControllerForId(m_layerId);
    m_layerAnimationController->addObserver(this);
}

LayerImpl::~LayerImpl()
{
#ifndef NDEBUG
    DCHECK(!m_betweenWillDrawAndDidDraw);
#endif
    m_layerTreeImpl->UnregisterLayer(this);
    m_layerAnimationController->removeObserver(this);
}

void LayerImpl::addChild(scoped_ptr<LayerImpl> child)
{
    child->setParent(this);
    DCHECK_EQ(layerTreeImpl(), child->layerTreeImpl());
    m_children.push_back(child.Pass());
    layerTreeImpl()->SetNeedsUpdateDrawProperties();
}

scoped_ptr<LayerImpl> LayerImpl::removeChild(LayerImpl* child)
{
    for (ScopedPtrVector<LayerImpl>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        if (*it == child) {
            scoped_ptr<LayerImpl> ret = m_children.take(it);
            m_children.erase(it);
            layerTreeImpl()->SetNeedsUpdateDrawProperties();
            return ret.Pass();
        }
    }
    return scoped_ptr<LayerImpl>();
}

void LayerImpl::removeAllChildren()
{
    m_children.clear();
    layerTreeImpl()->SetNeedsUpdateDrawProperties();
}

void LayerImpl::clearChildList()
{
    if (m_children.empty())
        return;

    m_children.clear();
    layerTreeImpl()->SetNeedsUpdateDrawProperties();
}

void LayerImpl::createRenderSurface()
{
    DCHECK(!m_drawProperties.render_surface);
    m_drawProperties.render_surface = make_scoped_ptr(new RenderSurfaceImpl(this));
    m_drawProperties.render_target = this;
}

scoped_ptr<SharedQuadState> LayerImpl::createSharedQuadState() const
{
  scoped_ptr<SharedQuadState> state = SharedQuadState::Create();
  state->SetAll(m_drawProperties.target_space_transform,
                m_drawProperties.visible_content_rect,
                m_drawProperties.drawable_content_rect,
                m_drawProperties.clip_rect,
                m_drawProperties.is_clipped,
                m_drawProperties.opacity);
  return state.Pass();
}

void LayerImpl::willDraw(ResourceProvider*)
{
#ifndef NDEBUG
    // willDraw/didDraw must be matched.
    DCHECK(!m_betweenWillDrawAndDidDraw);
    m_betweenWillDrawAndDidDraw = true;
#endif
}

void LayerImpl::didDraw(ResourceProvider*)
{
#ifndef NDEBUG
    DCHECK(m_betweenWillDrawAndDidDraw);
    m_betweenWillDrawAndDidDraw = false;
#endif
}

bool LayerImpl::showDebugBorders() const
{
    return layerTreeImpl()->debug_state().showDebugBorders;
}

void LayerImpl::getDebugBorderProperties(SkColor* color, float* width) const
{
    if (m_drawsContent) {
        *color = DebugColors::ContentLayerBorderColor();
        *width = DebugColors::ContentLayerBorderWidth(layerTreeImpl());
        return;
    }

    if (m_masksToBounds) {
        *color = DebugColors::MaskingLayerBorderColor();
        *width = DebugColors::MaskingLayerBorderWidth(layerTreeImpl());
        return;
    }

    *color = DebugColors::ContainerLayerBorderColor();
    *width = DebugColors::ContainerLayerBorderWidth(layerTreeImpl());
}

void LayerImpl::appendDebugBorderQuad(QuadSink& quadList, const SharedQuadState* sharedQuadState, AppendQuadsData& appendQuadsData) const
{
    if (!showDebugBorders())
        return;

    SkColor color;
    float width;
    getDebugBorderProperties(&color, &width);

    gfx::Rect contentRect(gfx::Point(), contentBounds());
    scoped_ptr<DebugBorderDrawQuad> debugBorderQuad = DebugBorderDrawQuad::Create();
    debugBorderQuad->SetNew(sharedQuadState, contentRect, color, width);
    quadList.append(debugBorderQuad.PassAs<DrawQuad>(), appendQuadsData);
}

bool LayerImpl::hasDelegatedContent() const
{
    return false;
}

bool LayerImpl::hasContributingDelegatedRenderPasses() const
{
    return false;
}

RenderPass::Id LayerImpl::firstContributingRenderPassId() const
{
    return RenderPass::Id(0, 0);
}

RenderPass::Id LayerImpl::nextContributingRenderPassId(RenderPass::Id) const
{
    return RenderPass::Id(0, 0);
}

ResourceProvider::ResourceId LayerImpl::contentsResourceId() const
{
    NOTREACHED();
    return 0;
}

void LayerImpl::setSentScrollDelta(const gfx::Vector2d& sentScrollDelta)
{
    // Pending tree never has sent scroll deltas
    DCHECK(layerTreeImpl()->IsActiveTree());

    if (m_sentScrollDelta == sentScrollDelta)
        return;

    m_sentScrollDelta = sentScrollDelta;
}

gfx::Vector2dF LayerImpl::scrollBy(const gfx::Vector2dF& scroll)
{
    gfx::Vector2dF minDelta = -m_scrollOffset;
    gfx::Vector2dF maxDelta = m_maxScrollOffset - m_scrollOffset;
    // Clamp newDelta so that position + delta stays within scroll bounds.
    gfx::Vector2dF newDelta = (m_scrollDelta + scroll);
    newDelta.ClampToMin(minDelta);
    newDelta.ClampToMax(maxDelta);
    gfx::Vector2dF unscrolled = m_scrollDelta + scroll - newDelta;

    setScrollDelta(newDelta);
    return unscrolled;
}

InputHandlerClient::ScrollStatus LayerImpl::tryScroll(const gfx::PointF& screenSpacePoint, InputHandlerClient::ScrollInputType type) const
{
    if (shouldScrollOnMainThread()) {
        TRACE_EVENT0("cc", "LayerImpl::tryScroll: Failed shouldScrollOnMainThread");
        return InputHandlerClient::ScrollOnMainThread;
    }

    if (!screenSpaceTransform().IsInvertible()) {
        TRACE_EVENT0("cc", "LayerImpl::tryScroll: Ignored nonInvertibleTransform");
        return InputHandlerClient::ScrollIgnored;
    }

    if (!nonFastScrollableRegion().IsEmpty()) {
        bool clipped = false;
        gfx::Transform inverseScreenSpaceTransform(gfx::Transform::kSkipInitialization);
        if (!screenSpaceTransform().GetInverse(&inverseScreenSpaceTransform)) {
            // TODO(shawnsingh): We shouldn't be applying a projection if screen space
            // transform is uninvertible here. Perhaps we should be returning
            // ScrollOnMainThread in this case?
        }

        gfx::PointF hitTestPointInContentSpace = MathUtil::projectPoint(inverseScreenSpaceTransform, screenSpacePoint, clipped);
        gfx::PointF hitTestPointInLayerSpace = gfx::ScalePoint(hitTestPointInContentSpace, 1 / contentsScaleX(), 1 / contentsScaleY());
        if (!clipped && nonFastScrollableRegion().Contains(gfx::ToRoundedPoint(hitTestPointInLayerSpace))) {
            TRACE_EVENT0("cc", "LayerImpl::tryScroll: Failed nonFastScrollableRegion");
            return InputHandlerClient::ScrollOnMainThread;
        }
    }

    if (type == InputHandlerClient::Wheel && haveWheelEventHandlers()) {
        TRACE_EVENT0("cc", "LayerImpl::tryScroll: Failed wheelEventHandlers");
        return InputHandlerClient::ScrollOnMainThread;
    }

    if (!scrollable()) {
        TRACE_EVENT0("cc", "LayerImpl::tryScroll: Ignored not scrollable");
        return InputHandlerClient::ScrollIgnored;
    }

    return InputHandlerClient::ScrollStarted;
}

bool LayerImpl::drawCheckerboardForMissingTiles() const
{
    return m_drawCheckerboardForMissingTiles && !layerTreeImpl()->settings().backgroundColorInsteadOfCheckerboard;
}

gfx::Rect LayerImpl::layerRectToContentRect(const gfx::RectF& layerRect) const
{
    gfx::RectF contentRect = gfx::ScaleRect(layerRect, contentsScaleX(), contentsScaleY());
    // Intersect with content rect to avoid the extra pixel because for some
    // values x and y, ceil((x / y) * y) may be x + 1.
    contentRect.Intersect(gfx::Rect(gfx::Point(), contentBounds()));
    return gfx::ToEnclosingRect(contentRect);
}

skia::RefPtr<SkPicture> LayerImpl::getPicture()
{
    return skia::RefPtr<SkPicture>();
}

bool LayerImpl::canClipSelf() const
{
    return false;
}

std::string LayerImpl::indentString(int indent)
{
    std::string str;
    for (int i = 0; i != indent; ++i)
        str.append("  ");
    return str;
}

void LayerImpl::dumpLayerProperties(std::string* str, int indent) const
{
    std::string indentStr = indentString(indent);
    str->append(indentStr);
    base::StringAppendF(str, "layer ID: %d\n", m_layerId);

    str->append(indentStr);
    base::StringAppendF(str, "bounds: %d, %d\n", bounds().width(), bounds().height());

    if (m_drawProperties.render_target) {
        str->append(indentStr);
        base::StringAppendF(str, "renderTarget: %d\n", m_drawProperties.render_target->m_layerId);
    }

    str->append(indentStr);
    base::StringAppendF(str, "position: %f, %f\n", m_position.x(), m_position.y());

    str->append(indentStr);
    base::StringAppendF(str, "contentsOpaque: %d\n", m_contentsOpaque);

    str->append(indentStr);
    const gfx::Transform& transform = m_drawProperties.target_space_transform;
    base::StringAppendF(str, "drawTransform: %f, %f, %f, %f  //  %f, %f, %f, %f  //  %f, %f, %f, %f  //  %f, %f, %f, %f\n",
        transform.matrix().getDouble(0, 0), transform.matrix().getDouble(0, 1), transform.matrix().getDouble(0, 2), transform.matrix().getDouble(0, 3),
        transform.matrix().getDouble(1, 0), transform.matrix().getDouble(1, 1), transform.matrix().getDouble(1, 2), transform.matrix().getDouble(1, 3),
        transform.matrix().getDouble(2, 0), transform.matrix().getDouble(2, 1), transform.matrix().getDouble(2, 2), transform.matrix().getDouble(2, 3),
        transform.matrix().getDouble(3, 0), transform.matrix().getDouble(3, 1), transform.matrix().getDouble(3, 2), transform.matrix().getDouble(3, 3));

    str->append(indentStr);
    base::StringAppendF(str, "drawsContent: %s\n", m_drawsContent ? "yes" : "no");
}

std::string LayerImpl::layerTreeAsText() const
{
    std::string str;
    dumpLayer(&str, 0);
    return str;
}

void LayerImpl::dumpLayer(std::string* str, int indent) const
{
    str->append(indentString(indent));
    base::StringAppendF(str, "%s(%s)\n", layerTypeAsString(), m_debugName.data());
    dumpLayerProperties(str, indent+2);
    if (m_replicaLayer) {
        str->append(indentString(indent+2));
        str->append("Replica:\n");
        m_replicaLayer->dumpLayer(str, indent+3);
    }
    if (m_maskLayer) {
        str->append(indentString(indent+2));
        str->append("Mask:\n");
        m_maskLayer->dumpLayer(str, indent+3);
    }
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->dumpLayer(str, indent+1);
}

base::DictionaryValue* LayerImpl::layerTreeAsJson() const
{
    base::ListValue* list;
    base::DictionaryValue* result = new base::DictionaryValue;
    result->SetString("LayerType", layerTypeAsString());

    list = new base::ListValue;
    list->AppendInteger(bounds().width());
    list->AppendInteger(bounds().height());
    result->Set("Bounds", list);

    list = new base::ListValue;
    list->AppendDouble(m_position.x());
    list->AppendDouble(m_position.y());
    result->Set("Position", list);

    const gfx::Transform& gfxTransform = m_drawProperties.target_space_transform;
    double transform[16];
    gfxTransform.matrix().asColMajord(transform);
    list = new base::ListValue;
    for (int i = 0; i < 16; ++i)
      list->AppendDouble(transform[i]);
    result->Set("DrawTransform", list);

    result->SetBoolean("DrawsContent", m_drawsContent);
    result->SetDouble("Opacity", opacity());

    list = new base::ListValue;
    for (size_t i = 0; i < m_children.size(); ++i)
        list->Append(m_children[i]->layerTreeAsJson());
    result->Set("Children", list);

    return result;
}

void LayerImpl::setStackingOrderChanged(bool stackingOrderChanged)
{
    // We don't need to store this flag; we only need to track that the change occurred.
    if (stackingOrderChanged)
        noteLayerPropertyChangedForSubtree();
}

bool LayerImpl::layerSurfacePropertyChanged() const
{
    if (m_layerSurfacePropertyChanged)
        return true;

    // If this layer's surface property hasn't changed, we want to see if
    // some layer above us has changed this property. This is done for the
    // case when such parent layer does not draw content, and therefore will
    // not be traversed by the damage tracker. We need to make sure that
    // property change on such layer will be caught by its descendants.
    LayerImpl* current = this->m_parent;
    while (current && !current->m_drawProperties.render_surface) {
        if (current->m_layerSurfacePropertyChanged)
            return true;
        current = current->m_parent;
    }

    return false;
}

void LayerImpl::noteLayerSurfacePropertyChanged()
{
    m_layerSurfacePropertyChanged = true;
    layerTreeImpl()->SetNeedsUpdateDrawProperties();
}

void LayerImpl::noteLayerPropertyChanged()
{
    m_layerPropertyChanged = true;
    layerTreeImpl()->SetNeedsUpdateDrawProperties();
}

void LayerImpl::noteLayerPropertyChangedForSubtree()
{
    noteLayerPropertyChanged();
    noteLayerPropertyChangedForDescendants();
}

void LayerImpl::noteLayerPropertyChangedForDescendants()
{
    layerTreeImpl()->SetNeedsUpdateDrawProperties();
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->noteLayerPropertyChangedForSubtree();
}

const char* LayerImpl::layerTypeAsString() const
{
    return "Layer";
}

void LayerImpl::resetAllChangeTrackingForSubtree()
{
    m_layerPropertyChanged = false;
    m_layerSurfacePropertyChanged = false;

    m_updateRect = gfx::RectF();

    if (m_drawProperties.render_surface)
        m_drawProperties.render_surface->resetPropertyChangedFlag();

    if (m_maskLayer)
        m_maskLayer->resetAllChangeTrackingForSubtree();

    if (m_replicaLayer)
        m_replicaLayer->resetAllChangeTrackingForSubtree(); // also resets the replica mask, if it exists.

    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->resetAllChangeTrackingForSubtree();
}

bool LayerImpl::layerIsAlwaysDamaged() const
{
    return false;
}

int LayerImpl::id() const
{
     return m_layerId;
}

void LayerImpl::OnOpacityAnimated(float opacity)
{
    setOpacity(opacity);
}

void LayerImpl::OnTransformAnimated(const gfx::Transform& transform)
{
    setTransform(transform);
}

bool LayerImpl::IsActive() const
{
    return m_layerTreeImpl->IsActiveTree();
}

void LayerImpl::setBounds(const gfx::Size& bounds)
{
    if (m_bounds == bounds)
        return;

    m_bounds = bounds;

    if (masksToBounds())
        noteLayerPropertyChangedForSubtree();
    else
        noteLayerPropertyChanged();
}

void LayerImpl::setMaskLayer(scoped_ptr<LayerImpl> maskLayer)
{
    int newLayerId = maskLayer ? maskLayer->id() : -1;

    if (maskLayer) {
        DCHECK_EQ(layerTreeImpl(), maskLayer->layerTreeImpl());
        DCHECK_NE(newLayerId, m_maskLayerId);
    } else if (newLayerId == m_maskLayerId)
        return;

    m_maskLayer = maskLayer.Pass();
    m_maskLayerId = newLayerId;
    noteLayerPropertyChangedForSubtree();
}

scoped_ptr<LayerImpl> LayerImpl::takeMaskLayer()
{
  m_maskLayerId = -1;
  return m_maskLayer.Pass();
}

void LayerImpl::setReplicaLayer(scoped_ptr<LayerImpl> replicaLayer)
{
    int newLayerId = replicaLayer ? replicaLayer->id() : -1;

    if (replicaLayer) {
        DCHECK_EQ(layerTreeImpl(), replicaLayer->layerTreeImpl());
        DCHECK_NE(newLayerId, m_replicaLayerId);
    } else if (newLayerId == m_replicaLayerId)
        return;

    m_replicaLayer = replicaLayer.Pass();
    m_replicaLayerId = newLayerId;
    noteLayerPropertyChangedForSubtree();
}

scoped_ptr<LayerImpl> LayerImpl::takeReplicaLayer()
{
  m_replicaLayerId = -1;
  return m_replicaLayer.Pass();
}

void LayerImpl::setDrawsContent(bool drawsContent)
{
    if (m_drawsContent == drawsContent)
        return;

    m_drawsContent = drawsContent;
    noteLayerPropertyChanged();
}

void LayerImpl::setAnchorPoint(const gfx::PointF& anchorPoint)
{
    if (m_anchorPoint == anchorPoint)
        return;

    m_anchorPoint = anchorPoint;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setAnchorPointZ(float anchorPointZ)
{
    if (m_anchorPointZ == anchorPointZ)
        return;

    m_anchorPointZ = anchorPointZ;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setBackgroundColor(SkColor backgroundColor)
{
    if (m_backgroundColor == backgroundColor)
        return;

    m_backgroundColor = backgroundColor;
    noteLayerPropertyChanged();
}

void LayerImpl::setFilters(const WebKit::WebFilterOperations& filters)
{
    if (m_filters == filters)
        return;

    DCHECK(!m_filter);
    m_filters = filters;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setBackgroundFilters(const WebKit::WebFilterOperations& backgroundFilters)
{
    if (m_backgroundFilters == backgroundFilters)
        return;

    m_backgroundFilters = backgroundFilters;
    noteLayerPropertyChanged();
}

void LayerImpl::setFilter(const skia::RefPtr<SkImageFilter>& filter)
{
    if (m_filter.get() == filter.get())
        return;

    DCHECK(m_filters.isEmpty());
    m_filter = filter;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setMasksToBounds(bool masksToBounds)
{
    if (m_masksToBounds == masksToBounds)
        return;

    m_masksToBounds = masksToBounds;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setContentsOpaque(bool opaque)
{
    if (m_contentsOpaque == opaque)
        return;

    m_contentsOpaque = opaque;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setOpacity(float opacity)
{
    if (m_opacity == opacity)
        return;

    m_opacity = opacity;
    noteLayerSurfacePropertyChanged();
}

float LayerImpl::opacity() const
{
    return m_opacity;
}

bool LayerImpl::opacityIsAnimating() const
{
    return m_layerAnimationController->isAnimatingProperty(Animation::Opacity);
}

void LayerImpl::setPosition(const gfx::PointF& position)
{
    if (m_position == position)
        return;

    m_position = position;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setPreserves3D(bool preserves3D)
{
    if (m_preserves3D == preserves3D)
        return;

    m_preserves3D = preserves3D;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setSublayerTransform(const gfx::Transform& sublayerTransform)
{
    if (m_sublayerTransform == sublayerTransform)
        return;

    m_sublayerTransform = sublayerTransform;
    // sublayer transform does not affect the current layer; it affects only its children.
    noteLayerPropertyChangedForDescendants();
}

void LayerImpl::setTransform(const gfx::Transform& transform)
{
    if (m_transform == transform)
        return;

    m_transform = transform;
    noteLayerSurfacePropertyChanged();
}

const gfx::Transform& LayerImpl::transform() const
{
    return m_transform;
}

bool LayerImpl::transformIsAnimating() const
{
    return m_layerAnimationController->isAnimatingProperty(Animation::Transform);
}

void LayerImpl::setContentBounds(const gfx::Size& contentBounds)
{
    if (this->contentBounds() == contentBounds)
        return;

    m_drawProperties.content_bounds = contentBounds;
    noteLayerPropertyChanged();
}

void LayerImpl::setContentsScale(float contentsScaleX, float contentsScaleY)
{
    if (this->contentsScaleX() == contentsScaleX && this->contentsScaleY() == contentsScaleY)
        return;

    m_drawProperties.contents_scale_x = contentsScaleX;
    m_drawProperties.contents_scale_y = contentsScaleY;
    noteLayerPropertyChanged();
}

void LayerImpl::calculateContentsScale(
    float idealContentsScale,
    float* contentsScaleX,
    float* contentsScaleY,
    gfx::Size* contentBounds)
{
    // Base LayerImpl has all of its content scales and content bounds pushed
    // from its Layer during commit and just reuses those values as-is.
    *contentsScaleX = this->contentsScaleX();
    *contentsScaleY = this->contentsScaleY();
    *contentBounds = this->contentBounds();
}

void LayerImpl::setScrollOffset(gfx::Vector2d scrollOffset)
{
    if (m_scrollOffset == scrollOffset)
        return;

    m_scrollOffset = scrollOffset;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setScrollDelta(const gfx::Vector2dF& scrollDelta)
{
    if (m_scrollDelta == scrollDelta)
        return;

    if (layerTreeImpl()->IsActiveTree())
    {
        LayerImpl* pending_twin = layerTreeImpl()->FindPendingTreeLayerById(id());
        if (pending_twin) {
            // The pending twin can't mirror the scroll delta of the active
            // layer.  Although the delta - sent scroll delta difference is
            // identical for both twins, the sent scroll delta for the pending
            // layer is zero, as anything that has been sent has been baked
            // into the layer's position/scroll offset as a part of commit.
            DCHECK(pending_twin->sentScrollDelta().IsZero());
            pending_twin->setScrollDelta(scrollDelta - sentScrollDelta());
        }
    }

    m_scrollDelta = scrollDelta;
    if (m_scrollbarAnimationController)
        m_scrollbarAnimationController->updateScrollOffset(this);
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setImplTransform(const gfx::Transform& transform)
{
    if (m_implTransform == transform)
        return;

    m_implTransform = transform;
    noteLayerPropertyChangedForSubtree();
}

void LayerImpl::setDoubleSided(bool doubleSided)
{
    if (m_doubleSided == doubleSided)
        return;

    m_doubleSided = doubleSided;
    noteLayerPropertyChangedForSubtree();
}

Region LayerImpl::visibleContentOpaqueRegion() const
{
    if (contentsOpaque())
        return visibleContentRect();
    return Region();
}

void LayerImpl::didLoseOutputSurface()
{
}

void LayerImpl::setMaxScrollOffset(gfx::Vector2d maxScrollOffset)
{
    if (m_maxScrollOffset == maxScrollOffset)
        return;
    m_maxScrollOffset = maxScrollOffset;

    layerTreeImpl()->SetNeedsUpdateDrawProperties();

    if (!m_scrollbarAnimationController)
        return;
    m_scrollbarAnimationController->updateScrollOffset(this);
}

ScrollbarLayerImpl* LayerImpl::horizontalScrollbarLayer()
{
    return m_scrollbarAnimationController ? m_scrollbarAnimationController->horizontalScrollbarLayer() : 0;
}

const ScrollbarLayerImpl* LayerImpl::horizontalScrollbarLayer() const
{
    return m_scrollbarAnimationController ? m_scrollbarAnimationController->horizontalScrollbarLayer() : 0;
}

inline scoped_ptr<ScrollbarAnimationController> createScrollbarAnimationControllerWithFade(LayerImpl* layer)
{
    double fadeoutDelay = 0.3;
    double fadeoutLength = 0.3;
    return ScrollbarAnimationControllerLinearFade::create(layer, fadeoutDelay, fadeoutLength).PassAs<ScrollbarAnimationController>();
}

void LayerImpl::setHorizontalScrollbarLayer(ScrollbarLayerImpl* scrollbarLayer)
{
    if (!m_scrollbarAnimationController) {
        if (m_layerTreeImpl->settings().useLinearFadeScrollbarAnimator)
            m_scrollbarAnimationController = createScrollbarAnimationControllerWithFade(this);
        else
            m_scrollbarAnimationController = ScrollbarAnimationController::create(this);
    }
    m_scrollbarAnimationController->setHorizontalScrollbarLayer(scrollbarLayer);
    m_scrollbarAnimationController->updateScrollOffset(this);
}

ScrollbarLayerImpl* LayerImpl::verticalScrollbarLayer()
{
    return m_scrollbarAnimationController ? m_scrollbarAnimationController->verticalScrollbarLayer() : 0;
}

const ScrollbarLayerImpl* LayerImpl::verticalScrollbarLayer() const
{
    return m_scrollbarAnimationController ? m_scrollbarAnimationController->verticalScrollbarLayer() : 0;
}

void LayerImpl::setVerticalScrollbarLayer(ScrollbarLayerImpl* scrollbarLayer)
{
    if (!m_scrollbarAnimationController) {
        if (m_layerTreeImpl->settings().useLinearFadeScrollbarAnimator)
            m_scrollbarAnimationController = createScrollbarAnimationControllerWithFade(this);
        else
            m_scrollbarAnimationController = ScrollbarAnimationController::create(this);
    }
    m_scrollbarAnimationController->setVerticalScrollbarLayer(scrollbarLayer);
    m_scrollbarAnimationController->updateScrollOffset(this);
}

}  // namespace cc
