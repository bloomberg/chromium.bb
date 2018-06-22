/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_ENCODER_PUSTATS_H_
#define AV1_ENCODER_PUSTATS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/ml.h"

#define NUM_FEATURES 12
#define NUM_HIDDEN_LAYERS 2
#define HIDDEN_LAYERS_0_NODES 10
#define HIDDEN_LAYERS_1_NODES 10
#define LOGITS_NODES 1

static const float
    av1_pustats_rate_hiddenlayer_0_kernel[NUM_FEATURES *
                                          HIDDEN_LAYERS_0_NODES] = {
      -52.9848f, -3.8058f,  -0.0282f, -0.0054f, 3.5367f,   -0.9193f,
      -0.9668f,  -0.9938f,  -0.9016f, -1.6092f, 5.3714f,   -57.6916f,
      101.6548f, 24.1252f,  0.0247f,  0.0021f,  12.5817f,  0.0826f,
      0.0865f,   0.0863f,   0.0851f,  -0.3443f, -0.0030f,  125.1990f,
      8.0394f,   33.9608f,  -0.1035f, -0.0418f, -13.6075f, 0.0522f,
      0.0570f,   0.0555f,   0.0539f,  -0.1967f, 0.1307f,   48.7033f,
      6.1298f,   -14.1249f, -1.1600f, -0.1019f, -15.8744f, -0.0388f,
      0.0577f,   -0.1058f,  -0.0021f, -0.1957f, 0.0631f,   14.2254f,
      -50.5693f, 14.3733f,  0.4533f,  -0.0918f, -2.1714f,  -0.0448f,
      -0.0633f,  -0.0821f,  -2.7087f, 0.1475f,  0.5090f,   -6.2023f,
      -67.2794f, 7.3298f,   0.0942f,  -0.1726f, -48.4015f, 0.3683f,
      0.3698f,   0.3700f,   0.3686f,  -1.4749f, 0.0488f,   -48.5977f,
      10.8774f,  -2.2353f,  -0.4552f, -0.1260f, -4.4067f,  -0.0928f,
      -0.1082f,  -0.4576f,  -0.2744f, 0.0842f,  0.0779f,   11.0408f,
      -29.7863f, 35.6904f,  0.0205f,  0.0020f,  -0.0657f,  0.1323f,
      0.1330f,   0.1340f,   0.1356f,  -0.5741f, -0.0124f,  -56.8193f,
      -0.1225f,  -0.2490f,  -0.3611f, -0.5065f, 0.0671f,   0.0470f,
      -0.3119f,  0.0031f,   -0.3892f, 0.0209f,  -0.3374f,  0.3125f,
      -84.2314f, 26.4263f,  -0.5345f, -0.0701f, -2.9544f,  -2.8927f,
      0.0248f,   0.0265f,   0.0326f,  -0.2016f, 0.3555f,   -81.5284f,
    };

static const float av1_pustats_rate_hiddenlayer_0_bias[HIDDEN_LAYERS_0_NODES] =
    {
      49.6087f,  -97.7676f, 63.262f,  -60.7973f, 37.9911f,
      114.9542f, -36.5848f, 44.9115f, 0.f,       -19.0169f,
    };

static const float
    av1_pustats_rate_hiddenlayer_1_kernel[HIDDEN_LAYERS_0_NODES *
                                          HIDDEN_LAYERS_1_NODES] = {
      -0.0319f, -0.5428f, -1.0271f, -0.5222f, -0.5177f, -0.1878f, -0.7222f,
      -0.3883f, -0.2759f, -0.3861f, -0.2250f, -0.1830f, 0.0911f,  0.0099f,
      -0.0941f, 0.2999f,  -0.2001f, -0.1672f, -0.4494f, -0.6745f, -0.9414f,
      -0.3108f, -0.4178f, -0.0250f, -1.5476f, -0.7486f, -0.0949f, -1.0776f,
      -0.1162f, -0.0393f, -0.2844f, -1.5018f, 1.0457f,  -0.4873f, 0.0520f,
      0.0325f,  -0.7639f, 0.0901f,  -0.4397f, -0.2217f, -0.3215f, 0.0028f,
      0.1817f,  -0.3343f, -0.0285f, 0.0747f,  -0.4363f, -0.1085f, 0.0262f,
      0.0557f,  -0.0426f, -0.0010f, -0.1310f, -0.0373f, -0.0478f, 0.8759f,
      0.3227f,  -0.3028f, -0.1272f, 0.1649f,  -0.0218f, 0.0020f,  0.4598f,
      -0.5867f, -0.1339f, -0.2585f, -0.5198f, -0.3986f, 0.3847f,  -0.9327f,
      -2.9352f, -1.3549f, 0.0275f,  -0.3291f, 0.0399f,  1.7515f,  -0.4562f,
      -0.5511f, -0.1604f, -5.1808f, -0.5439f, -0.1537f, 0.0408f,  0.0433f,
      -0.1239f, 0.0982f,  -0.1869f, 0.0367f,  0.1234f,  -0.1579f, -6.5508f,
      -4.2091f, 0.4521f,  -0.1081f, 1.1255f,  -0.1830f, 0.1175f,  3.0831f,
      -0.0361f, -7.9087f,
    };

static const float av1_pustats_rate_hiddenlayer_1_bias[HIDDEN_LAYERS_1_NODES] =
    {
      -0.487f,  73.2381f,  -7.2221f,  67.742f,  -94.2112f,
      86.6603f, -78.5546f, -53.7831f, 74.8688f, 62.3852f,
    };

static const float
    av1_pustats_rate_logits_kernel[HIDDEN_LAYERS_1_NODES * LOGITS_NODES] = {
      0.0934f, 5.1931f,  0.0059f,  1.9797f, -7.2527f,
      3.175f,  -3.5956f, -0.7743f, 3.6156f, -0.4359f,
    };

static const float av1_pustats_rate_logits_bias[LOGITS_NODES] = {
  0.0f,
};

static const NN_CONFIG av1_pustats_rate_nnconfig = {
  NUM_FEATURES,                                      // num_inputs
  LOGITS_NODES,                                      // num_outputs
  NUM_HIDDEN_LAYERS,                                 // num_hidden_layers
  { HIDDEN_LAYERS_0_NODES, HIDDEN_LAYERS_1_NODES },  // num_hidden_nodes
  {
      av1_pustats_rate_hiddenlayer_0_kernel,
      av1_pustats_rate_hiddenlayer_1_kernel,
      av1_pustats_rate_logits_kernel,
  },
  {
      av1_pustats_rate_hiddenlayer_0_bias,
      av1_pustats_rate_hiddenlayer_1_bias,
      av1_pustats_rate_logits_bias,
  },
};

static const float
    av1_pustats_dist_hiddenlayer_0_kernel[NUM_FEATURES *
                                          HIDDEN_LAYERS_0_NODES] = {
      -156.5319f, -11.3994f, -0.3982f, 0.0487f,  -0.4955f,  0.0068f,
      0.0185f,    0.0113f,   -0.0150f, -0.0243f, 6.0052f,   -154.2997f,
      -130.2849f, -5.1616f,  -0.3404f, -0.3211f, 4.2239f,   -0.0557f,
      -0.0491f,   -0.0619f,  -0.0713f, 0.2668f,  2.9735f,   -149.7339f,
      292.2589f,  16.8577f,  1.3207f,  0.0352f,  -8.0571f,  0.2060f,
      1.1488f,    -2.0189f,  -0.0837f, -0.2732f, -1.2496f,  240.5130f,
      225.3297f,  29.3146f,  -2.5422f, 0.0463f,  -9.9347f,  0.9695f,
      -0.7304f,   0.6714f,   -1.1186f, -0.6366f, -1.9709f,  300.3602f,
      240.5540f,  60.0422f,  0.2836f,  0.0628f,  -0.5733f,  -0.0581f,
      -0.0423f,   -0.0078f,  -0.1029f, 0.3488f,  -1.6991f,  229.6841f,
      -16.8176f,  36.3221f,  -2.6667f, 0.0854f,  1.0294f,   -4.5821f,
      -14.8688f,  1.3588f,   0.3744f,  -2.0388f, 1.2277f,   -53.6839f,
      -262.1413f, -45.3783f, -0.5114f, 0.0142f,  0.0410f,   0.0299f,
      0.0269f,    0.0398f,   0.0239f,  -0.1086f, -0.0795f,  -261.1547f,
      283.0113f,  -14.5801f, -0.2325f, 0.0090f,  -11.1228f, 0.0079f,
      0.0017f,    0.0015f,   0.0060f,  -0.0567f, 1.7072f,   278.6905f,
      -283.2938f, -23.4154f, 1.8506f,  0.0228f,  -0.3758f,  -0.0860f,
      -0.1069f,   -0.1119f,  -0.0686f, 0.2715f,  0.3776f,   -264.6542f,
      243.3052f,  16.9577f,  -2.3714f, 0.0302f,  -6.9604f,  -0.7748f,
      -0.1843f,   0.2813f,   0.5952f,  -0.2351f, -1.0892f,  228.8468f,
    };

static const float av1_pustats_dist_hiddenlayer_0_bias[HIDDEN_LAYERS_0_NODES] =
    {
      171.487f, 201.9275f, -87.6005f,  -30.4912f, -84.2485f,
      -3.8174f, 117.2134f, -154.7031f, 94.6126f,  -105.7839f,
    };

static const float
    av1_pustats_dist_hiddenlayer_1_kernel[HIDDEN_LAYERS_0_NODES *
                                          HIDDEN_LAYERS_1_NODES] = {
      -0.1263f,  -0.2110f,  0.0110f,  -0.0049f,  0.0413f,  -0.4399f,  -3.6666f,
      0.7959f,   -0.1598f,  -0.0066f, 1.0107f,   -0.2067f, -0.4900f,  -0.2354f,
      -0.7138f,  0.1107f,   4.8429f,  -3.5367f,  1.3956f,  -0.6637f,  0.3391f,
      -28.9262f, -0.8041f,  -1.1152f, -0.7234f,  0.0634f,  6.3529f,   -3.4083f,
      3.6488f,   -1.6298f,  0.4895f,  -12.1619f, 0.2704f,  0.0880f,   -0.1572f,
      0.0453f,   6.0672f,   -3.9104f, 1.0770f,   0.1611f,  1.2132f,   -1.0541f,
      0.0443f,   0.0384f,   -1.9548f, -4.2956f,  -7.3207f, -2.4772f,  0.9465f,
      0.1716f,   0.5436f,   -0.0758f, -0.0470f,  0.0406f,  -0.9711f,  -0.0578f,
      -1.6807f,  -2.0604f,  0.0375f,  0.0034f,   0.5545f,  -0.9050f,  -0.0640f,
      -0.1102f,  -0.0548f,  -0.1699f, 3.7530f,   -0.3292f, -0.9793f,  -0.0972f,
      0.5857f,   -95.0965f, 0.0666f,  -0.3055f,  -0.8357f, 0.0633f,   5.9729f,
      -2.5916f,  2.5703f,   -0.2228f, -0.1960f,  -0.6505f, -0.2052f,  -0.2105f,
      -0.6469f,  -0.7796f,  -0.0199f, -0.2458f,  -1.1055f, -0.0684f,  1.1432f,
      -0.3940f,  -0.9575f,  -1.4032f, 0.0078f,   0.3299f,  -12.1073f, -22.0621f,
      -1.2747f,  -2.5620f,
    };

static const float av1_pustats_dist_hiddenlayer_1_bias[HIDDEN_LAYERS_1_NODES] =
    {
      -260.9098f, 116.9876f, -95.0241f, -138.0498f, 121.9383f,
      213.1319f,  197.8458f, 87.5999f,  -3.5863f,   64.4119f,
    };

static const float
    av1_pustats_dist_logits_kernel[HIDDEN_LAYERS_1_NODES * LOGITS_NODES] = {
      -9.2865f, 1.3821f, -1.0596f, -1.386f,  0.7778f,
      -2.6082f, 1.2962f, 1.5822f,  -0.0722f, 0.2201f,
    };

static const float av1_pustats_dist_logits_bias[LOGITS_NODES] = {
  0.0f,
};

static const NN_CONFIG av1_pustats_dist_nnconfig = {
  NUM_FEATURES,                                      // num_inputs
  LOGITS_NODES,                                      // num_outputs
  NUM_HIDDEN_LAYERS,                                 // num_hidden_layers
  { HIDDEN_LAYERS_0_NODES, HIDDEN_LAYERS_1_NODES },  // num_hidden_nodes
  {
      av1_pustats_dist_hiddenlayer_0_kernel,
      av1_pustats_dist_hiddenlayer_1_kernel,
      av1_pustats_dist_logits_kernel,
  },
  {
      av1_pustats_dist_hiddenlayer_0_bias,
      av1_pustats_dist_hiddenlayer_1_bias,
      av1_pustats_dist_logits_bias,
  },
};

#undef NUM_FEATURES
#undef NUM_HIDDEN_LAYERS
#undef HIDDEN_LAYERS_0_NODES
#undef HIDDEN_LAYERS_1_NODES
#undef LOGITS_NODES

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_PUSTATS_H_
