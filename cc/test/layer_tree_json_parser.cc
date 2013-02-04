// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_json_parser.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "cc/content_layer.h"
#include "cc/layer.h"
#include "cc/nine_patch_layer.h"
#include "cc/solid_color_layer.h"

namespace cc {

namespace {

scoped_refptr<Layer> ParseTreeFromValue(base::Value* val,
                                        ContentLayerClient* content_client) {
  DictionaryValue* dict;
  bool success = true;
  success &= val->GetAsDictionary(&dict);
  std::string layer_type;
  success &= dict->GetString("LayerType", &layer_type);
  ListValue* list;
  success &= dict->GetList("Bounds", &list);
  int width, height;
  success &= list->GetInteger(0, &width);
  success &= list->GetInteger(1, &height);
  success &= dict->GetList("Position", &list);
  double position_x, position_y;
  success &= list->GetDouble(0, &position_x);
  success &= list->GetDouble(1, &position_y);

  bool draws_content;
  success &= dict->GetBoolean("DrawsContent", &draws_content);

  scoped_refptr<Layer> new_layer;
  if (layer_type == "SolidColorLayer") {
    new_layer = SolidColorLayer::create();
  } else if (layer_type == "ContentLayer") {
    new_layer = ContentLayer::create(content_client);
  } else if (layer_type == "NinePatchLayer") {
    success &= dict->GetList("ImageAperture", &list);
    int aperture_x, aperture_y, aperture_width, aperture_height;
    success &= list->GetInteger(0, &aperture_x);
    success &= list->GetInteger(1, &aperture_y);
    success &= list->GetInteger(2, &aperture_width);
    success &= list->GetInteger(3, &aperture_height);

    success &= dict->GetList("ImageBounds", &list);
    int image_width, image_height;
    success &= list->GetInteger(0, &image_width);
    success &= list->GetInteger(1, &image_height);

    scoped_refptr<NinePatchLayer> nine_patch_layer = NinePatchLayer::create();

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, image_width, image_height);
    bitmap.allocPixels(NULL, NULL);
    nine_patch_layer->setBitmap(bitmap,
        gfx::Rect(aperture_x, aperture_y, aperture_width, aperture_height));

    new_layer = nine_patch_layer;
  } else {  // Type "Layer" or "unknown"
    new_layer = Layer::create();
  }
  new_layer->setAnchorPoint(gfx::Point());
  new_layer->setPosition(gfx::PointF(position_x, position_y));
  new_layer->setBounds(gfx::Size(width, height));
  new_layer->setIsDrawable(draws_content);

  double opacity;
  if (dict->GetDouble("Opacity", &opacity))
    new_layer->setOpacity(opacity);

  success &= dict->GetList("DrawTransform", &list);
  double transform[16];
  for (int i = 0; i < 16; ++i)
    success &= list->GetDouble(i, &transform[i]);

  gfx::Transform gfxTransform;
  gfxTransform.matrix().setColMajord(transform);
  new_layer->setTransform(gfxTransform);

  success &= dict->GetList("Children", &list);
  for (ListValue::const_iterator it = list->begin();
       it != list->end(); ++it) {
    new_layer->addChild(ParseTreeFromValue(*it, content_client));
  }

  if (!success)
    return NULL;

  return new_layer;
}

}  // namespace

scoped_refptr<Layer> ParseTreeFromJson(std::string json,
                                       ContentLayerClient* content_client) {
  scoped_ptr<base::Value> val = base::test::ParseJson(json);
  return ParseTreeFromValue(val.get(), content_client);
}

}  // namespace cc
