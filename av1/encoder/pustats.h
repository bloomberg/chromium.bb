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

#define NUM_FEATURES 11
#define NUM_HIDDEN_LAYERS 2
#define HIDDEN_LAYERS_0_NODES 12
#define HIDDEN_LAYERS_1_NODES 10
#define LOGITS_NODES 1

static const float
    av1_pustats_rate_hiddenlayer_0_kernel[NUM_FEATURES *
                                          HIDDEN_LAYERS_0_NODES] = {
      38.2328f,  13.9989f,  0.0061f,   -0.3183f,  -0.0583f,  -0.0535f,
      -0.0487f,  -0.0525f,  0.1909f,   -0.0392f,  31.3349f,  0.3925f,
      -13.1046f, -0.2474f,  -24.7553f, -0.0331f,  -0.0578f,  -0.0916f,
      -0.0807f,  0.0925f,   0.0690f,   4.2632f,   -8.4528f,  -1.3344f,
      -0.0306f,  -0.3650f,  -0.0369f,  -0.1127f,  -0.0387f,  -1.0365f,
      0.1646f,   0.2134f,   -9.1568f,  17.4768f,  -5.1957f,  0.0051f,
      -16.1172f, -0.1033f,  -0.0393f,  -0.0232f,  0.0062f,   0.1084f,
      0.1531f,   19.9729f,  -18.4704f, 2.4077f,   -0.2724f,  -13.7616f,
      0.0398f,   0.0352f,   0.0208f,   0.0324f,   -0.1707f,  0.0215f,
      -14.6161f, -23.8785f, 2.8513f,   -0.1804f,  -10.5403f, -0.0570f,
      -0.0595f,  -0.0692f,  -0.0725f,  0.2702f,   0.0550f,   -22.2600f,
      -15.0236f, -1.7400f,  -0.0947f,  0.2542f,   -0.0062f,  -0.0134f,
      -0.0144f,  -0.0136f,  0.0457f,   0.1987f,   -13.2386f, -22.4104f,
      4.5122f,   -0.0011f,  -6.4581f,  -0.0280f,  -0.0395f,  -0.0465f,
      -0.0419f,  0.0952f,   0.0291f,   -21.3738f, -16.5887f, 6.4565f,
      0.0515f,   5.1318f,   -0.0156f,  -0.0108f,  -0.0351f,  -0.0001f,
      0.0589f,   -0.2822f,  -10.4313f, 6.0403f,   10.5999f,  -0.1395f,
      0.4011f,   -0.0181f,  0.0156f,   -1.4002f,  -0.0362f,  0.0896f,
      0.2268f,   4.0045f,   21.9254f,  -2.3134f,  0.0153f,   -4.3785f,
      -0.0433f,  -0.0724f,  -0.1152f,  -0.1209f,  0.4315f,   0.0109f,
      25.4771f,  -16.4259f, 12.1237f,  0.0034f,   0.0771f,   -0.0883f,
      -0.0927f,  -0.0433f,  -0.0539f,  -0.0062f,  0.0114f,   -18.0060f,
    };

static const float av1_pustats_rate_hiddenlayer_0_bias[HIDDEN_LAYERS_0_NODES] =
    {
      -21.4915f, -44.4869f, 41.1231f, -48.0328f, 37.8403f,  49.7133f,
      43.089f,   41.5194f,  -5.4191f, -18.2127f, -42.3387f, -8.3132f,
    };

static const float
    av1_pustats_rate_hiddenlayer_1_kernel[HIDDEN_LAYERS_0_NODES *
                                          HIDDEN_LAYERS_1_NODES] = {
      -0.6574f, -0.1131f, -0.1042f, -0.3099f, -0.2533f, 0.1681f,  0.6068f,
      0.4092f,  -0.3041f, -0.0940f, -0.2101f, 1.0094f,  -0.5015f, -0.0597f,
      -0.5085f, -0.4512f, -0.4430f, 0.1479f,  -0.1513f, -0.0512f, -0.0915f,
      0.0547f,  -0.7939f, -0.2708f, -0.4007f, 0.1317f,  0.0831f,  -0.0244f,
      0.7654f,  0.5626f,  0.5902f,  -0.9345f, -0.0016f, -0.2309f, -0.0028f,
      -0.7078f, 0.8577f,  -0.4490f, 0.0564f,  -0.0105f, -0.6304f, 0.1990f,
      0.1812f,  0.1073f,  -0.3936f, -0.2738f, 0.0294f,  -0.5651f, 0.0032f,
      -0.4394f, -0.2906f, -0.1826f, 0.6235f,  0.4284f,  0.3801f,  1.3111f,
      -0.1588f, -0.1721f, -0.1548f, -0.5448f, -0.0946f, -0.1144f, 0.3432f,
      0.0800f,  0.1918f,  0.3935f,  0.1803f,  -0.0968f, -0.0357f, -0.0231f,
      0.4395f,  1.2799f,  0.0025f,  -0.2027f, -0.2624f, 0.0261f,  0.8387f,
      0.3995f,  0.2748f,  0.3571f,  -0.0287f, -0.0453f, 0.0702f,  -0.5144f,
      0.6494f,  -0.0236f, -0.2983f, 0.1152f,  0.2835f,  0.3472f,  0.0213f,
      0.1056f,  -0.7108f, -0.5502f, 0.1598f,  -0.0014f, -0.2568f, -0.4455f,
      -0.2046f, -0.2023f, -0.0836f, 0.3572f,  0.0860f,  -0.0074f, -0.3819f,
      -0.0343f, -0.2259f, -0.0566f, 0.2179f,  -0.0092f, -0.1808f, 0.1282f,
      0.5541f,  0.4327f,  0.4352f,  -0.2827f, -0.2944f, -0.1872f, 0.1638f,
      -0.5893f,
    };

static const float av1_pustats_rate_hiddenlayer_1_bias[HIDDEN_LAYERS_1_NODES] =
    {
      -37.4903f, -4.4543f, 48.3478f, -28.9432f, 38.8565f,
      -34.7689f, 41.9086f, 42.2953f, 39.6133f,  -51.5207f,
    };

static const float
    av1_pustats_rate_logits_kernel[HIDDEN_LAYERS_1_NODES * LOGITS_NODES] = {
      -3.1724f, -0.1484f, 3.8965f, -3.815f, 2.9538f,
      -0.274f,  2.4407f,  2.2758f, 8.3852f, -4.5312f,
    };

static const float av1_pustats_rate_logits_bias[LOGITS_NODES] = {
  28.5203f,
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
      10.0046f, -1.1045f,  0.2987f,   -0.4514f, -0.1875f,  0.0584f,  0.2031f,
      0.0232f,  -0.1214f,  -3.2002f,  15.9990f, -5.1910f,  -3.9390f, 0.0568f,
      0.2814f,  -0.3486f,  -0.5531f,  -0.6320f, -0.7021f,  -0.3323f, -0.0245f,
      -9.2214f, -30.5593f, -15.6657f, 0.0179f,  -0.0349f,  0.0589f,  0.0606f,
      0.0716f,  0.0577f,   -0.2615f,  -0.0768f, -36.3785f, -2.6594f, -2.7904f,
      -0.4688f, 0.2131f,   0.0950f,   -0.1039f, -0.3842f,  0.0762f,  -0.0522f,
      -0.3180f, -3.0266f,  5.9289f,   2.1566f,  -0.1810f,  0.1808f,  0.0840f,
      0.0789f,  0.0865f,   0.0774f,   -0.3407f, 0.5262f,   -3.1397f, 5.5787f,
      10.7935f, 0.0529f,   0.0606f,   -1.0615f, -1.2050f,  -1.7885f, -1.4508f,
      -0.8030f, 2.0140f,   7.4947f,   8.2245f,  0.9503f,   -0.5658f, 0.5364f,
      -0.0809f, -0.0851f,  -0.0990f,  -0.1036f, 0.1067f,   0.2817f,  11.5333f,
      34.8357f, 2.7020f,   0.0654f,   -1.1930f, -0.0798f,  -0.0953f, -0.0970f,
      -0.1007f, 0.3953f,   0.4299f,   36.1065f, 12.0585f,  1.3723f,  0.2179f,
      -0.4137f, -0.2750f,  -0.2307f,  -0.2025f, -0.2749f,  -0.0285f, -2.4707f,
      21.3095f, -20.2470f, 0.4493f,   0.2219f,  -0.2301f,  -0.0637f, 0.0024f,
      0.0576f,  -0.0214f,  0.0445f,   -1.0108f, -28.4280f, -9.1785f, -1.3002f,
      0.3442f,  0.0028f,   -0.6972f,  -0.7735f, -0.7904f,  -0.8322f, -0.7644f,
      -1.4834f, -12.8910f, 0.2646f,   -1.5534f, -0.2966f,  0.5710f,  -0.1023f,
      -0.0463f, 0.0608f,   -0.0511f,  0.0079f,  -0.7601f,  1.2548f,
    };

static const float av1_pustats_dist_hiddenlayer_0_bias[HIDDEN_LAYERS_0_NODES] =
    {
      -11.0757f, 15.3936f, -2.326f,   -2.4645f, 12.3655f, -8.3979f,
      13.3447f,  -1.9968f, -22.9272f, 10.565f,  18.4379f, 0.5819f,
    };

static const float
    av1_pustats_dist_hiddenlayer_1_kernel[HIDDEN_LAYERS_0_NODES *
                                          HIDDEN_LAYERS_1_NODES] = {
      -0.6357f, -0.3517f, -0.2552f, 0.1066f,  -0.8766f, 0.4122f,  0.0529f,
      -0.3544f, 0.8226f,  -0.2646f, 0.1726f,  -0.1339f, 0.0346f,  -0.0273f,
      -0.2446f, 0.1922f,  0.1769f,  0.0081f,  0.1185f,  0.1987f,  -0.3075f,
      -0.1364f, -0.0752f, 0.2092f,  -0.2332f, 0.5176f,  0.2288f,  -0.3271f,
      -3.0885f, -1.5484f, 0.4302f,  -0.7231f, -0.3321f, 0.1343f,  0.4097f,
      -0.6404f, -0.1637f, 0.0138f,  -0.4233f, 0.2872f,  -0.4548f, 0.6054f,
      -0.3799f, -0.3675f, 0.5597f,  -0.2739f, -0.1764f, 0.5522f,  0.1532f,
      0.0156f,  -0.8675f, 0.4320f,  1.0300f,  -0.9162f, 2.2395f,  0.6111f,
      0.0816f,  -0.3566f, -0.0315f, 0.1262f,  0.0270f,  0.3043f,  -0.3479f,
      0.2470f,  -4.8841f, -0.9909f, -0.1614f, -0.7017f, -1.4157f, -0.0859f,
      0.1149f,  -0.9783f, -0.3250f, 0.4990f,  0.1225f,  0.1236f,  3.1213f,
      0.2334f,  1.8604f,  -4.2287f, 0.1196f,  0.1623f,  -0.0905f, 0.0160f,
      -1.0164f, 0.1588f,  0.0575f,  0.2458f,  0.2021f,  -0.0546f, 0.3279f,
      0.0183f,  -0.7507f, -0.3528f, -1.6896f, -0.2439f, 0.1030f,  0.0794f,
      -0.4565f, 0.3375f,  0.6047f,  0.0850f,  0.2428f,  0.5032f,  0.1905f,
      -0.3696f, -0.0682f, 0.2283f,  -0.0499f, -0.9781f, 0.1160f,  -0.3202f,
      0.3849f,  0.1463f,  -0.1136f, -0.8909f, 0.0072f,  0.2319f,  -0.1521f,
      -0.4638f,
    };

static const float av1_pustats_dist_hiddenlayer_1_bias[HIDDEN_LAYERS_1_NODES] =
    {
      5.182f,  -18.2182f, 17.1402f,  8.1365f,  10.7104f,
      2.3185f, -10.8856f, -15.2689f, 11.4082f, -9.6543f,
    };

static const float
    av1_pustats_dist_logits_kernel[HIDDEN_LAYERS_1_NODES * LOGITS_NODES] = {
      -0.1842f, 0.517f,   0.0422f, 0.1247f,  -0.0354f,
      0.2806f,  -0.0442f, 0.2223f, -0.1674f, 0.0587f,
    };

static const float av1_pustats_dist_logits_bias[LOGITS_NODES] = {
  16.0801f,
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
