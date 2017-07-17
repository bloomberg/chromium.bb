#include "av1/common/daala_tx.h"
#include "av1/common/odintrin.h"

/* clang-format off */

# define OD_DCT_RSHIFT(_a, _b) OD_UNBIASED_RSHIFT32(_a, _b)

/* TODO: Daala DCT overflow checks need to be ported as a later test */
# if defined(OD_DCT_CHECK_OVERFLOW)
# else
#  define OD_DCT_OVERFLOW_CHECK(val, scale, offset, idx)
# endif

#define OD_FDCT_2(p0, p1) \
  /* Embedded 2-point orthonormal Type-II fDCT. */ \
  do { \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 13573, 16384, 100); \
    p0 -= (p1*13573 + 16384) >> 15; \
    /* 5793/8192 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(p0, 5793, 4096, 101); \
    p1 += (p0*5793 + 4096) >> 13; \
    /* 3393/8192 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 3393, 4096, 102); \
    p0 -= (p1*3393 + 4096) >> 13; \
  } \
  while (0)

#define OD_IDCT_2(p0, p1) \
  /* Embedded 2-point orthonormal Type-II iDCT. */ \
  do { \
    /* 3393/8192 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    p0 += (p1*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    p1 -= (p0*5793 + 4096) >> 13; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    p0 += (p1*13573 + 16384) >> 15; \
  } \
  while (0)

#define OD_FDCT_2_ASYM(p0, p1, p1h) \
  /* Embedded 2-point asymmetric Type-II fDCT. */ \
  do { \
    p0 += p1h; \
    p1 = p0 - p1; \
  } \
  while (0)

#define OD_IDCT_2_ASYM(p0, p1, p1h) \
  /* Embedded 2-point asymmetric Type-II iDCT. */ \
  do { \
    p1 = p0 - p1; \
    p1h = OD_DCT_RSHIFT(p1, 1); \
    p0 -= p1h; \
  } \
  while (0)

#define OD_FDST_2(p0, p1) \
  /* Embedded 2-point orthonormal Type-IV fDST. */ \
  do { \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 10947, 8192, 103); \
    p0 -= (p1*10947 + 8192) >> 14; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(p0, 473, 256, 104); \
    p1 += (p0*473 + 256) >> 9; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 10947, 8192, 105); \
    p0 -= (p1*10947 + 8192) >> 14; \
  } \
  while (0)

#define OD_IDST_2(p0, p1) \
  /* Embedded 2-point orthonormal Type-IV iDST. */ \
  do { \
    /* 10947/16384 ~= Tan[3*Pi/16]) ~= 0.668178637919299 */ \
    p0 += (p1*10947 + 8192) >> 14; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    p1 -= (p0*473 + 256) >> 9; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    p0 += (p1*10947 + 8192) >> 14; \
  } \
  while (0)

#define OD_FDST_2_ASYM(p0, p1) \
  /* Embedded 2-point asymmetric Type-IV fDST. */ \
  do { \
    /* 11507/16384 ~= 4*Sin[Pi/8] - 2*Tan[Pi/8] ~= 0.702306604714169 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 11507, 8192, 187); \
    p0 -= (p1*11507 + 8192) >> 14; \
    /* 669/1024 ~= Cos[Pi/8]/Sqrt[2] ~= 0.653281482438188 */ \
    OD_DCT_OVERFLOW_CHECK(p0, 669, 512, 188); \
    p1 += (p0*669 + 512) >> 10; \
    /* 4573/4096 ~= 4*Sin[Pi/8] - Tan[Pi/8] ~= 1.11652016708726 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 4573, 2048, 189); \
    p0 -= (p1*4573 + 2048) >> 12; \
  } \
  while (0)

#define OD_IDST_2_ASYM(p0, p1) \
  /* Embedded 2-point asymmetric Type-IV iDST. */ \
  do { \
    /* 4573/4096 ~= 4*Sin[Pi/8] - Tan[Pi/8] ~= 1.11652016708726 */ \
    p0 += (p1*4573 + 2048) >> 12; \
    /* 669/1024 ~= Cos[Pi/8]/Sqrt[2] ~= 0.653281482438188 */ \
    p1 -= (p0*669 + 512) >> 10; \
    /* 11507/16384 ~= 4*Sin[Pi/8] - 2*Tan[Pi/8] ~= 0.702306604714169 */ \
    p0 += (p1*11507 + 8192) >> 14; \
  } \
  while (0)

#define OD_FDCT_4(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-II fDCT. */ \
  do { \
    int q2h; \
    int q3h; \
    q3 = q0 - q3; \
    q3h = OD_DCT_RSHIFT(q3, 1); \
    q0 -= q3h; \
    q2 += q1; \
    q2h = OD_DCT_RSHIFT(q2, 1); \
    q1 = q2h - q1; \
    OD_FDCT_2_ASYM(q0, q2, q2h); \
    OD_FDST_2_ASYM(q3, q1); \
  } \
  while (0)

#define OD_IDCT_4(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-II iDCT. */ \
  do { \
    int q1h; \
    int q3h; \
    OD_IDST_2_ASYM(q3, q2); \
    OD_IDCT_2_ASYM(q0, q1, q1h); \
    q3h = OD_DCT_RSHIFT(q3, 1); \
    q0 += q3h; \
    q3 = q0 - q3; \
    q2 = q1h - q2; \
    q1 -= q2; \
  } \
  while (0)

#define OD_FDCT_4_ASYM(q0, q2, q2h, q1, q3, q3h) \
  /* Embedded 4-point asymmetric Type-II fDCT. */ \
  do { \
    q0 += q3h; \
    q3 = q0 - q3; \
    q1 = q2h - q1; \
    q2 = q1 - q2; \
    OD_FDCT_2(q0, q2); \
    OD_FDST_2(q3, q1); \
  } \
  while (0)

#define OD_IDCT_4_ASYM(q0, q2, q1, q1h, q3, q3h) \
  /* Embedded 4-point asymmetric Type-II iDCT. */ \
  do { \
    OD_IDST_2(q3, q2); \
    OD_IDCT_2(q0, q1); \
    q1 = q2 - q1; \
    q1h = OD_DCT_RSHIFT(q1, 1); \
    q2 = q1h - q2; \
    q3 = q0 - q3; \
    q3h = OD_DCT_RSHIFT(q3, 1); \
    q0 -= q3h; \
  } \
  while (0)

#define OD_FDST_4(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-IV fDST. */ \
  do { \
    int q0h; \
    int q1h; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(q1, 13573, 16384, 190); \
    q2 += (q1*13573 + 16384) >> 15; \
    /* 5793/8192 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(q2, 5793, 4096, 191); \
    q1 -= (q2*5793 + 4096) >> 13; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(q1, 3393, 4096, 192); \
    q2 += (q1*3393 + 4096) >> 13; \
    q0 += q2; \
    q0h = OD_DCT_RSHIFT(q0, 1); \
    q2 = q0h - q2; \
    q1 += q3; \
    q1h = OD_DCT_RSHIFT(q1, 1); \
    q3 -= q1h; \
    /* 537/1024 ~= (1/Sqrt[2] - Cos[3*Pi/16]/2)/Sin[3*Pi/16] ~=
        0.524455699240090 */ \
    OD_DCT_OVERFLOW_CHECK(q1, 537, 512, 193); \
    q2 -= (q1*537 + 512) >> 10; \
    /* 1609/2048 ~= Sqrt[2]*Sin[3*Pi/16] ~= 0.785694958387102 */ \
    OD_DCT_OVERFLOW_CHECK(q2, 1609, 1024, 194); \
    q1 += (q2*1609 + 1024) >> 11; \
    /* 7335/32768 ~= (1/Sqrt[2] - Cos[3*Pi/16])/Sin[3*Pi/16] ~=
        0.223847182092655 */ \
    OD_DCT_OVERFLOW_CHECK(q1, 7335, 16384, 195); \
    q2 += (q1*7335 + 16384) >> 15; \
    /* 5091/8192 ~= (1/Sqrt[2] - Cos[7*Pi/16]/2)/Sin[7*Pi/16] ~=
        0.6215036383171189 */ \
    OD_DCT_OVERFLOW_CHECK(q0, 5091, 4096, 196); \
    q3 += (q0*5091 + 4096) >> 13; \
    /* 5681/4096 ~= Sqrt[2]*Sin[7*Pi/16] ~= 1.38703984532215 */ \
    OD_DCT_OVERFLOW_CHECK(q3, 5681, 2048, 197); \
    q0 -= (q3*5681 + 2048) >> 12; \
    /* 4277/8192 ~= (1/Sqrt[2] - Cos[7*Pi/16])/Sin[7*Pi/16] ~=
        0.52204745462729 */ \
    OD_DCT_OVERFLOW_CHECK(q0, 4277, 4096, 198); \
    q3 += (q0*4277 + 4096) >> 13; \
  } \
  while (0)

#define OD_IDST_4(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-IV iDST. */ \
  do { \
    int q0h; \
    int q2h; \
    /* 4277/8192 ~= (1/Sqrt[2] - Cos[7*Pi/16])/Sin[7*Pi/16] ~=
        0.52204745462729 */ \
    q3 -= (q0*4277 + 4096) >> 13; \
    /* 5681/4096 ~= Sqrt[2]*Sin[7*Pi/16] ~= 1.38703984532215 */ \
    q0 += (q3*5681 + 2048) >> 12; \
    /* 5091/8192 ~= (1/Sqrt[2] - Cos[7*Pi/16]/2)/Sin[7*Pi/16] ~=
        0.6215036383171189 */ \
    q3 -= (q0*5091 + 4096) >> 13; \
    /* 7335/32768 ~= (1/Sqrt[2] - Cos[3*Pi/16])/Sin[3*Pi/16] ~=
        0.223847182092655 */ \
    q1 -= (q2*7335 + 16384) >> 15; \
    /* 1609/2048 ~= Sqrt[2]*Sin[3*Pi/16] ~= 0.785694958387102 */ \
    q2 -= (q1*1609 + 1024) >> 11; \
    /* 537/1024 ~= (1/Sqrt[2] - Cos[3*Pi/16]/2)/Sin[3*Pi/16] ~=
        0.524455699240090 */ \
    q1 += (q2*537 + 512) >> 10; \
    q2h = OD_DCT_RSHIFT(q2, 1); \
    q3 += q2h; \
    q2 -= q3; \
    q0h = OD_DCT_RSHIFT(q0, 1); \
    q1 = q0h - q1; \
    q0 -= q1; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    q1 -= (q2*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    q2 += (q1*5793 + 4096) >> 13; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    q1 -= (q2*13573 + 16384) >> 15; \
  } \
  while (0)

#define OD_FDST_4_ASYM(t0, t0h, t2, t1, t3) \
  /* Embedded 4-point asymmetric Type-IV fDST. */ \
  do { \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 7489, 4096, 106); \
    t2 -= (t1*7489 + 4096) >> 13; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 11585, 8192, 107); \
    t1 += (t2*11585 + 8192) >> 14; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 19195, 16384, 108); \
    t2 += (t1*19195 + 16384) >> 15; \
    t3 += OD_DCT_RSHIFT(t2, 1); \
    t2 -= t3; \
    t1 = t0h - t1; \
    t0 -= t1; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(t0, 6723, 4096, 109); \
    t3 += (t0*6723 + 4096) >> 13; \
    /* 8035/8192 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 8035, 4096, 110); \
    t0 -= (t3*8035 + 4096) >> 13; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(t0, 6723, 4096, 111); \
    t3 += (t0*6723 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 8757, 8192, 112); \
    t2 += (t1*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 6811, 4096, 113); \
    t1 -= (t2*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 8757, 8192, 114); \
    t2 += (t1*8757 + 8192) >> 14; \
  } \
  while (0)

#define OD_IDST_4_ASYM(t0, t0h, t2, t1, t3) \
  /* Embedded 4-point asymmetric Type-IV iDST. */ \
  do { \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    t1 -= (t2*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    t2 += (t1*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    t1 -= (t2*8757 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    t3 -= (t0*6723 + 4096) >> 13; \
    /* 8035/8192 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    t0 += (t3*8035 + 4096) >> 13; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    t3 -= (t0*6723 + 4096) >> 13; \
    t0 += t2; \
    t0h = OD_DCT_RSHIFT(t0, 1); \
    t2 = t0h - t2; \
    t1 += t3; \
    t3 -= OD_DCT_RSHIFT(t1, 1); \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    t1 -= (t2*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    t2 -= (t1*11585 + 8192) >> 14; \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    t1 += (t2*7489 + 4096) >> 13; \
  } \
  while (0)

#define OD_FDCT_8(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point orthonormal Type-II fDCT. */ \
  do { \
    int r4h; \
    int r5h; \
    int r6h; \
    int r7h; \
    r7 = r0 - r7; \
    r7h = OD_DCT_RSHIFT(r7, 1); \
    r0 -= r7h; \
    r6 += r1; \
    r6h = OD_DCT_RSHIFT(r6, 1); \
    r1 = r6h - r1; \
    r5 = r2 - r5; \
    r5h = OD_DCT_RSHIFT(r5, 1); \
    r2 -= r5h; \
    r4 += r3; \
    r4h = OD_DCT_RSHIFT(r4, 1); \
    r3 = r4h - r3; \
    OD_FDCT_4_ASYM(r0, r4, r4h, r2, r6, r6h); \
    OD_FDST_4_ASYM(r7, r7h, r3, r5, r1); \
  } \
  while (0)

#define OD_IDCT_8(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point orthonormal Type-II iDCT. */ \
  do { \
    int r1h; \
    int r3h; \
    int r5h; \
    int r7h; \
    OD_IDST_4_ASYM(r7, r7h, r5, r6, r4); \
    OD_IDCT_4_ASYM(r0, r2, r1, r1h, r3, r3h); \
    r0 += r7h; \
    r7 = r0 - r7; \
    r6 = r1h - r6; \
    r1 -= r6; \
    r5h = OD_DCT_RSHIFT(r5, 1); \
    r2 += r5h; \
    r5 = r2 - r5; \
    r4 = r3h - r4; \
    r3 -= r4; \
  } \
  while (0)

#define OD_FDCT_8_ASYM(r0, r4, r4h, r2, r6, r6h, r1, r5, r5h, r3, r7, r7h) \
  /* Embedded 8-point asymmetric Type-II fDCT. */ \
  do { \
    r0 += r7h; \
    r7 = r0 - r7; \
    r1 = r6h - r1; \
    r6 -= r1; \
    r2 += r5h; \
    r5 = r2 - r5; \
    r3 = r4h - r3; \
    r4 -= r3; \
    OD_FDCT_4(r0, r4, r2, r6); \
    OD_FDST_4(r7, r3, r5, r1); \
  } \
  while (0)

#define OD_IDCT_8_ASYM(r0, r4, r2, r6, r1, r1h, r5, r5h, r3, r3h, r7, r7h) \
  /* Embedded 8-point asymmetric Type-II iDCT. */ \
  do { \
    OD_IDST_4(r7, r5, r6, r4); \
    OD_IDCT_4(r0, r2, r1, r3); \
    r7 = r0 - r7; \
    r7h = OD_DCT_RSHIFT(r7, 1); \
    r0 -= r7h; \
    r1 += r6; \
    r1h = OD_DCT_RSHIFT(r1, 1); \
    r6 = r1h - r6; \
    r5 = r2 - r5; \
    r5h = OD_DCT_RSHIFT(r5, 1); \
    r2 -= r5h; \
    r3 += r4; \
    r3h = OD_DCT_RSHIFT(r3, 1); \
    r4 = r3h - r4; \
  } \
  while (0)

#define OD_FDST_8(t0, t4, t2, t6, t1, t5, t3, t7)  \
  /* Embedded 8-point orthonormal Type-IV fDST. */ \
  do { \
    int t0h; \
    int t2h; \
    int t5h; \
    int t7h; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 13573, 16384, 115); \
    t6 -= (t1*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 11585, 8192, 116); \
    t1 += (t6*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 13573, 16384, 117); \
    t6 -= (t1*13573 + 16384) >> 15; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 21895, 16384, 118); \
    t5 -= (t2*21895 + 16384) >> 15; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 15137, 8192, 119); \
    t2 += (t5*15137 + 8192) >> 14; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 10947, 8192, 120); \
    t5 -= (t2*10947 + 8192) >> 14; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 3259, 8192, 121); \
    t4 -= (t3*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 3135, 4096, 122); \
    t3 += (t4*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 3259, 8192, 123); \
    t4 -= (t3*3259 + 8192) >> 14; \
    t7 += t1; \
    t7h = OD_DCT_RSHIFT(t7, 1); \
    t1 -= t7h; \
    t2 = t3 - t2; \
    t2h = OD_DCT_RSHIFT(t2, 1); \
    t3 -= t2h; \
    t0 -= t6; \
    t0h = OD_DCT_RSHIFT(t0, 1); \
    t6 += t0h; \
    t5 = t4 - t5; \
    t5h = OD_DCT_RSHIFT(t5, 1); \
    t4 -= t5h; \
    t1 += t5h; \
    t5 = t1 - t5; \
    t4 += t0h; \
    t0 -= t4; \
    t6 -= t2h; \
    t2 += t6; \
    t3 -= t7h; \
    t7 += t3; \
    /* TODO: Can we move this into another operation */ \
    t7 = -t7; \
    /* 7425/8192 ~= Tan[15*Pi/64] ~= 0.906347169019147 */ \
    OD_DCT_OVERFLOW_CHECK(t7, 7425, 4096, 124); \
    t0 -= (t7*7425 + 4096) >> 13; \
    /* 8153/8192 ~= Sin[15*Pi/32] ~= 0.995184726672197 */ \
    OD_DCT_OVERFLOW_CHECK(t0, 8153, 4096, 125); \
    t7 += (t0*8153 + 4096) >> 13; \
    /* 7425/8192 ~= Tan[15*Pi/64] ~= 0.906347169019147 */ \
    OD_DCT_OVERFLOW_CHECK(t7, 7425, 4096, 126); \
    t0 -= (t7*7425 + 4096) >> 13; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.148335987538347 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 4861, 16384, 127); \
    t6 -= (t1*4861 + 16384) >> 15; \
    /* 1189/4096 ~= Sin[3*Pi/32] ~= 0.290284677254462 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 1189, 2048, 128); \
    t1 += (t6*1189 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.148335987538347 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 4861, 16384, 129); \
    t6 -= (t1*4861 + 16384) >> 15; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.599376933681924 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 2455, 2048, 130); \
    t2 -= (t5*2455 + 2048) >> 12; \
    /* 7225/8192 ~= Sin[11*Pi/32] ~= 0.881921264348355 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 7225, 4096, 131); \
    t5 += (t2*7225 + 4096) >> 13; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.599376933681924 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 2455, 2048, 132); \
    t2 -= (t5*2455 + 2048) >> 12; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.357805721314524 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 11725, 16384, 133); \
    t4 -= (t3*11725 + 16384) >> 15; \
    /* 5197/8192 ~= Sin[7*Pi/32] ~= 0.634393284163645 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 5197, 4096, 134); \
    t3 += (t4*5197 + 4096) >> 13; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.357805721314524 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 11725, 16384, 135); \
    t4 -= (t3*11725 + 16384) >> 15; \
  } \
  while (0)

#define OD_IDST_8(t0, t4, t2, t6, t1, t5, t3, t7) \
  /* Embedded 8-point orthonormal Type-IV iDST. */ \
  do { \
    int t0h; \
    int t2h; \
    int t5h_; \
    int t7h_; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.357805721314524 */ \
    t1 += (t6*11725 + 16384) >> 15; \
    /* 5197/8192 ~= Sin[7*Pi/32] ~= 0.634393284163645 */ \
    t6 -= (t1*5197 + 4096) >> 13; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.357805721314524 */ \
    t1 += (t6*11725 + 16384) >> 15; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.599376933681924 */ \
    t2 += (t5*2455 + 2048) >> 12; \
    /* 7225/8192 ~= Sin[11*Pi/32] ~= 0.881921264348355 */ \
    t5 -= (t2*7225 + 4096) >> 13; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.599376933681924 */ \
    t2 += (t5*2455 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.148335987538347 */ \
    t3 += (t4*4861 + 16384) >> 15; \
    /* 1189/4096 ~= Sin[3*Pi/32] ~= 0.290284677254462 */ \
    t4 -= (t3*1189 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.148335987538347 */ \
    t3 += (t4*4861 + 16384) >> 15; \
    /* 7425/8192 ~= Tan[15*Pi/64] ~= 0.906347169019147 */ \
    t0 += (t7*7425 + 4096) >> 13; \
    /* 8153/8192 ~= Sin[15*Pi/32] ~= 0.995184726672197 */ \
    t7 -= (t0*8153 + 4096) >> 13; \
    /* 7425/8192 ~= Tan[15*Pi/64] ~= 0.906347169019147 */ \
    t0 += (t7*7425 + 4096) >> 13; \
    /* TODO: Can we move this into another operation */ \
    t7 = -t7; \
    t7 -= t6; \
    t7h_ = OD_DCT_RSHIFT(t7, 1); \
    t6 += t7h_; \
    t2 -= t3; \
    t2h = OD_DCT_RSHIFT(t2, 1); \
    t3 += t2h; \
    t0 += t1; \
    t0h = OD_DCT_RSHIFT(t0, 1); \
    t1 -= t0h; \
    t5 = t4 - t5; \
    t5h_ = OD_DCT_RSHIFT(t5, 1); \
    t4 -= t5h_; \
    t1 += t5h_; \
    t5 = t1 - t5; \
    t3 -= t0h; \
    t0 += t3; \
    t6 += t2h; \
    t2 = t6 - t2; \
    t4 += t7h_; \
    t7 -= t4; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    t1 += (t6*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    t6 -= (t1*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    t1 += (t6*3259 + 8192) >> 14; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t5 += (t2*10947 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t2 -= (t5*15137 + 8192) >> 14; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t5 += (t2*21895 + 16384) >> 15; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    t3 += (t4*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    t4 -= (t3*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    t3 += (t4*13573 + 16384) >> 15; \
  } \
  while (0)

/* Rewrite this so that t0h can be passed in. */
#define OD_FDST_8_ASYM(t0, t4, t2, t6, t1, t5, t3, t7) \
  /* Embedded 8-point asymmetric Type-IV fDST. */ \
  do { \
    int t0h; \
    int t2h; \
    int t5h; \
    int t7h; \
    /* 1035/2048 ~= (Sqrt[2] - Cos[7*Pi/32])/(2*Sin[7*Pi/32]) */ \
    OD_DCT_OVERFLOW_CHECK(t1, 1035, 1024, 199); \
    t6 += (t1*1035 + 1024) >> 11; \
    /* 3675/4096 ~= Sqrt[2]*Sin[7*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t6, 3675, 2048, 200); \
    t1 -= (t6*3675 + 2048) >> 12; \
    /* 851/8192 ~= (Cos[7*Pi/32] - 1/Sqrt[2])/Sin[7*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t1, 851, 4096, 201); \
    t6 -= (t1*851 + 4096) >> 13; \
    /* 4379/8192 ~= (Sqrt[2] - Sin[5*Pi/32])/(2*Cos[5*Pi/32]) */ \
    OD_DCT_OVERFLOW_CHECK(t2, 4379, 4096, 202); \
    t5 += (t2*4379 + 4096) >> 13; \
    /* 10217/8192 ~= Sqrt[2]*Cos[5*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t5, 10217, 4096, 203); \
    t2 -= (t5*10217 + 4096) >> 13; \
    /* 4379/16384 ~= (1/Sqrt[2] - Sin[5*Pi/32])/Cos[5*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t2, 4379, 8192, 204); \
    t5 += (t2*4379 + 8192) >> 14; \
    /* 12905/16384 ~= (Sqrt[2] - Cos[3*Pi/32])/(2*Sin[3*Pi/32]) */ \
    OD_DCT_OVERFLOW_CHECK(t3, 12905, 8192, 205); \
    t4 += (t3*12905 + 8192) >> 14; \
    /* 3363/8192 ~= Sqrt[2]*Sin[3*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t4, 3363, 4096, 206); \
    t3 -= (t4*3363 + 4096) >> 13; \
    /* 3525/4096 ~= (Cos[3*Pi/32] - 1/Sqrt[2])/Sin[3*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t3, 3525, 2048, 207); \
    t4 -= (t3*3525 + 2048) >> 12; \
    /* 5417/8192 ~= (Sqrt[2] - Sin[Pi/32])/(2*Cos[Pi/32]) */ \
    OD_DCT_OVERFLOW_CHECK(t0, 5417, 4096, 208); \
    t7 += (t0*5417 + 4096) >> 13; \
    /* 5765/4096 ~= Sqrt[2]*Cos[Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t7, 5765, 2048, 209); \
    t0 -= (t7*5765 + 2048) >> 12; \
    /* 2507/4096 ~= (1/Sqrt[2] - Sin[Pi/32])/Cos[Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t0, 2507, 2048, 210); \
    t7 += (t0*2507 + 2048) >> 12; \
    t0 += t1; \
    t0h = OD_DCT_RSHIFT(t0, 1); \
    t1 -= t0h; \
    t2 -= t3; \
    t2h = OD_DCT_RSHIFT(t2, 1); \
    t3 += t2h; \
    t5 -= t4; \
    t5h = OD_DCT_RSHIFT(t5, 1); \
    t4 += t5h; \
    t7 += t6; \
    t7h = OD_DCT_RSHIFT(t7, 1); \
    t6 = t7h - t6; \
    t4 = t7h - t4; \
    t7 -= t4; \
    t1 += t5h; \
    t5 = t1 - t5; \
    t6 += t2h; \
    t2 = t6 - t2; \
    t3 -= t0h; \
    t0 += t3; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 3259, 8192, 211); \
    t1 += (t6*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 3135, 4096, 212); \
    t6 -= (t1*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 3259, 8192, 213); \
    t1 += (t6*3259 + 8192) >> 14; \
    /* 2737/4096 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 2737, 2048, 214); \
    t5 += (t2*2737 + 2048) >> 12; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 473, 256, 215); \
    t2 -= (t5*473 + 256) >> 9; \
    /* 2737/4096 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 2737, 2048, 216); \
    t5 += (t2*2737 + 2048) >> 12; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 3393, 4096, 217); \
    t3 += (t4*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 5793, 4096, 218); \
    t4 -= (t3*5793 + 4096) >> 13; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 3393, 4096, 219); \
    t3 += (t4*3393 + 4096) >> 13; \
  } \
  while (0)

#define OD_IDST_8_ASYM(t0, t4, t2, t6, t1, t5, t3, t7) \
  /* Embedded 8-point asymmetric Type-IV iDST. */ \
  do { \
    int t0h; \
    int t2h; \
    int t5h__; \
    int t7h__; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    t6 -= (t1*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    t1 += (t6*5793 + 4096) >> 13; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    t6 -= (t1*3393 + 4096) >> 13; \
    /* 2737/4096 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t5 -= (t2*2737 + 2048) >> 12; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t2 += (t5*473 + 256) >> 9; \
    /* 2737/4096 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t5 -= (t2*2737 + 2048) >> 12; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    t4 -= (t3*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    t3 += (t4*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    t4 -= (t3*3259 + 8192) >> 14; \
    t0 -= t6; \
    t0h = OD_DCT_RSHIFT(t0, 1); \
    t6 += t0h; \
    t2 = t3 - t2; \
    t2h = OD_DCT_RSHIFT(t2, 1); \
    t3 -= t2h; \
    t5 = t4 - t5; \
    t5h__ = OD_DCT_RSHIFT(t5, 1); \
    t4 -= t5h__; \
    t7 += t1; \
    t7h__ = OD_DCT_RSHIFT(t7, 1); \
    t1 = t7h__ - t1; \
    t3 = t7h__ - t3; \
    t7 -= t3; \
    t1 -= t5h__; \
    t5 += t1; \
    t6 -= t2h; \
    t2 += t6; \
    t4 += t0h; \
    t0 -= t4; \
    /* 2507/4096 ~= (1/Sqrt[2] - Sin[Pi/32])/Cos[Pi/32] */ \
    t7 -= (t0*2507 + 2048) >> 12; \
    /* 5765/4096 ~= Sqrt[2]*Cos[Pi/32] */ \
    t0 += (t7*5765 + 2048) >> 12; \
    /* 5417/8192 ~= (Sqrt[2] - Sin[Pi/32])/(2*Cos[Pi/32]) */ \
    t7 -= (t0*5417 + 4096) >> 13; \
    /* 3525/4096 ~= (Cos[3*Pi/32] - 1/Sqrt[2])/Sin[3*Pi/32] */ \
    t1 += (t6*3525 + 2048) >> 12; \
    /* 3363/8192 ~= Sqrt[2]*Sin[3*Pi/32] */ \
    t6 += (t1*3363 + 4096) >> 13; \
    /* 12905/16384 ~= (1/Sqrt[2] - Cos[3*Pi/32]/1)/Sin[3*Pi/32] */ \
    t1 -= (t6*12905 + 8192) >> 14; \
    /* 4379/16384 ~= (1/Sqrt[2] - Sin[5*Pi/32])/Cos[5*Pi/32] */ \
    t5 -= (t2*4379 + 8192) >> 14; \
    /* 10217/8192 ~= Sqrt[2]*Cos[5*Pi/32] */ \
    t2 += (t5*10217 + 4096) >> 13; \
    /* 4379/8192 ~= (Sqrt[2] - Sin[5*Pi/32])/(2*Cos[5*Pi/32]) */ \
    t5 -= (t2*4379 + 4096) >> 13; \
    /* 851/8192 ~= (Cos[7*Pi/32] - 1/Sqrt[2])/Sin[7*Pi/32] */ \
    t3 += (t4*851 + 4096) >> 13; \
    /* 3675/4096 ~= Sqrt[2]*Sin[7*Pi/32] */ \
    t4 += (t3*3675 + 2048) >> 12; \
    /* 1035/2048 ~= (Sqrt[2] - Cos[7*Pi/32])/(2*Sin[7*Pi/32]) */ \
    t3 -= (t4*1035 + 1024) >> 11; \
  } \
  while (0)

#define OD_FDCT_16(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-II fDCT. */ \
  do { \
    int s8h; \
    int sah; \
    int sch; \
    int seh; \
    int sfh; \
    sf = s0 - sf; \
    sfh = OD_DCT_RSHIFT(sf, 1); \
    s0 -= sfh; \
    se += s1; \
    seh = OD_DCT_RSHIFT(se, 1); \
    s1 = seh - s1; \
    sd = s2 - sd; \
    s2 -= OD_DCT_RSHIFT(sd, 1); \
    sc += s3; \
    sch = OD_DCT_RSHIFT(sc, 1); \
    s3 = sch - s3; \
    sb = s4 - sb; \
    s4 -= OD_DCT_RSHIFT(sb, 1); \
    sa += s5; \
    sah = OD_DCT_RSHIFT(sa, 1); \
    s5 = sah - s5; \
    s9 = s6 - s9; \
    s6 -= OD_DCT_RSHIFT(s9, 1); \
    s8 += s7; \
    s8h = OD_DCT_RSHIFT(s8, 1); \
    s7 = s8h - s7; \
    OD_FDCT_8_ASYM(s0, s8, s8h, s4, sc, sch, s2, sa, sah, s6, se, seh); \
    OD_FDST_8_ASYM(sf, s7, sb, s3, sd, s5, s9, s1); \
  } \
  while (0)

#define OD_IDCT_16(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-II iDCT. */ \
  do { \
    int s1h; \
    int s3h; \
    int s5h; \
    int s7h; \
    int sfh; \
    OD_IDST_8_ASYM(sf, sb, sd, s9, se, sa, sc, s8); \
    OD_IDCT_8_ASYM(s0, s4, s2, s6, s1, s1h, s5, s5h, s3, s3h, s7, s7h); \
    sfh = OD_DCT_RSHIFT(sf, 1); \
    s0 += sfh; \
    sf = s0 - sf; \
    se = s1h - se; \
    s1 -= se; \
    s2 += OD_DCT_RSHIFT(sd, 1); \
    sd = s2 - sd; \
    sc = s3h - sc; \
    s3 -= sc; \
    s4 += OD_DCT_RSHIFT(sb, 1); \
    sb = s4 - sb; \
    sa = s5h - sa; \
    s5 -= sa; \
    s6 += OD_DCT_RSHIFT(s9, 1); \
    s9 = s6 - s9; \
    s8 = s7h - s8; \
    s7 -= s8; \
  } \
  while (0)

#define OD_FDST_16(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-IV fDST. */ \
  do { \
    int s0h; \
    int s2h; \
    int sdh; \
    int sfh; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s3, 13573, 16384, 220); \
    s1 += (se*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(s1, 11585, 8192, 221); \
    se -= (s1*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s3, 13573, 16384, 222); \
    s1 += (se*13573 + 16384) >> 15; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(s2, 21895, 16384, 223); \
    sd += (s2*21895 + 16384) >> 15; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(sd, 15137, 16384, 224); \
    s2 -= (sd*15137 + 8192) >> 14; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(s2, 21895, 16384, 225); \
    sd += (s2*21895 + 16384) >> 15; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(s3, 3259, 8192, 226); \
    sc += (s3*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    OD_DCT_OVERFLOW_CHECK(sc, 3135, 4096, 227); \
    s3 -= (sc*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(s3, 3259, 8192, 228); \
    sc += (s3*3259 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s5, 13573, 16384, 229); \
    sa += (s5*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(sa, 11585, 8192, 230); \
    s5 -= (sa*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s5, 13573, 16384, 231); \
    sa += (s5*13573 + 16384) >> 15; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s9, 13573, 16384, 232); \
    s6 += (s9*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(s6, 11585, 8192, 233); \
    s9 -= (s6*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s9, 13573, 16384, 234); \
    s6 += (s9*13573 + 16384) >> 15; \
    sf += se; \
    sfh = OD_DCT_RSHIFT(sf, 1); \
    se = sfh - se; \
    s0 += s1; \
    s0h = OD_DCT_RSHIFT(s0, 1); \
    s1 = s0h - s1; \
    s2 = s3 - s2; \
    s2h = OD_DCT_RSHIFT(s2, 1); \
    s3 -= s2h; \
    sd -= sc; \
    sdh = OD_DCT_RSHIFT(sd, 1); \
    sc += sdh; \
    sa = s4 - sa; \
    s4 -= OD_DCT_RSHIFT(sa, 1); \
    s5 += sb; \
    sb = OD_DCT_RSHIFT(s5, 1) - sb; \
    s8 += s6; \
    s6 -= OD_DCT_RSHIFT(s8, 1); \
    s7 = s9 - s7; \
    s9 -= OD_DCT_RSHIFT(s7, 1); \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(sb, 6723, 4096, 235); \
    s4 += (sb*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    OD_DCT_OVERFLOW_CHECK(s4, 16069, 8192, 236); \
    sb -= (s4*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32]) ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(sb, 6723, 4096, 237); \
    s4 += (sb*6723 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32]) ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(s5, 8757, 8192, 238); \
    sa += (s5*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    OD_DCT_OVERFLOW_CHECK(sa, 6811, 4096, 239); \
    s5 -= (sa*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(s5, 8757, 8192, 240); \
    sa += (s5*8757 + 8192) >> 14; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    OD_DCT_OVERFLOW_CHECK(s9, 2485, 4096, 241); \
    s6 += (s9*2485 + 4096) >> 13; \
    /* 4551/8192 ~= Sin[3*Pi/16] ~= 0.555570233019602 */ \
    OD_DCT_OVERFLOW_CHECK(s6, 4551, 4096, 242); \
    s9 -= (s6*4551 + 4096) >> 13; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    OD_DCT_OVERFLOW_CHECK(s9, 2485, 4096, 243); \
    s6 += (s9*2485 + 4096) >> 13; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    OD_DCT_OVERFLOW_CHECK(s8, 3227, 16384, 244); \
    s7 += (s8*3227 + 16384) >> 15; \
    /* 6393/32768 ~= Sin[Pi/16] ~= 0.19509032201612825 */ \
    OD_DCT_OVERFLOW_CHECK(s7, 6393, 16384, 245); \
    s8 -= (s7*6393 + 16384) >> 15; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    OD_DCT_OVERFLOW_CHECK(s8, 3227, 16384, 246); \
    s7 += (s8*3227 + 16384) >> 15; \
    s1 -= s2h; \
    s2 += s1; \
    se += sdh; \
    sd = se - sd; \
    s3 += sfh; \
    sf -= s3; \
    sc = s0h - sc; \
    s0 -= sc; \
    sb += OD_DCT_RSHIFT(s8, 1); \
    s8 = sb - s8; \
    s4 += OD_DCT_RSHIFT(s7, 1); \
    s7 -= s4; \
    s6 += OD_DCT_RSHIFT(s5, 1); \
    s5 = s6 - s5; \
    s9 -= OD_DCT_RSHIFT(sa, 1); \
    sa += s9; \
    s8 += s0; \
    s0 -= OD_DCT_RSHIFT(s8, 1); \
    sf += s7; \
    s7 = OD_DCT_RSHIFT(sf, 1) - s7; \
    s1 -= s6; \
    s6 += OD_DCT_RSHIFT(s1, 1); \
    s9 += se; \
    se = OD_DCT_RSHIFT(s9, 1) - se; \
    s2 += sa; \
    sa = OD_DCT_RSHIFT(s2, 1) - sa; \
    s5 += sd; \
    sd -= OD_DCT_RSHIFT(s5, 1); \
    s4 = sc - s4; \
    sc -= OD_DCT_RSHIFT(s4, 1); \
    s3 -= sb; \
    sb += OD_DCT_RSHIFT(s3, 1); \
    /* 2799/4096 ~= (1/Sqrt[2] - Cos[31*Pi/64]/2)/Sin[31*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(sf, 2799, 2048, 247); \
    s0 -= (sf*2799 + 2048) >> 12; \
    /* 2893/2048 ~= Sqrt[2]*Sin[31*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s0, 2893, 1024, 248); \
    sf += (s0*2893 + 1024) >> 11; \
    /* 5397/8192 ~= (Cos[Pi/4] - Cos[31*Pi/64])/Sin[31*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(sf, 5397, 4096, 249); \
    s0 -= (sf*5397 + 4096) >> 13; \
    /* 41/64 ~= (1/Sqrt[2] - Cos[29*Pi/64]/2)/Sin[29*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s1, 41, 32, 250); \
    se += (s1*41 + 32) >> 6; \
    /* 2865/2048 ~= Sqrt[2]*Sin[29*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(se, 2865, 1024, 251); \
    s1 -= (se*2865 + 1024) >> 11; \
    /* 4641/8192 ~= (1/Sqrt[2] - Cos[29*Pi/64])/Sin[29*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s1, 4641, 4096, 252); \
    se += (s1*4641 + 4096) >> 13; \
    /* 2473/4096 ~= (1/Sqrt[2] - Cos[27*Pi/64]/2)/Sin[27*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s2, 2473, 2048, 253); \
    sd += (s2*2473 + 2048) >> 12; \
    /* 5619/4096 ~= Sqrt[2]*Sin[27*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(sd, 5619, 2048, 254); \
    s2 -= (sd*5619 + 2048) >> 12; \
    /* 7839/16384 ~= (1/Sqrt[2] - Cos[27*Pi/64])/Sin[27*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s2, 7839, 8192, 255); \
    sd += (s2*7839 + 8192) >> 14; \
    /* 5747/8192 ~= (1/Sqrt[2] - Cos[7*Pi/64]/2)/Sin[7*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s3, 5747, 4096, 256); \
    sc -= (s3*5747 + 4096) >> 13; \
    /* 3903/8192 ~= Sqrt[2]*Sin[7*Pi/64] ~= */ \
    OD_DCT_OVERFLOW_CHECK(sc, 3903, 4096, 257); \
    s3 += (sc*3903 + 4096) >> 13; \
    /* 5701/8192 ~= (1/Sqrt[2] - Cos[7*Pi/64])/Sin[7*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s3, 5701, 4096, 258); \
    sc += (s3*5701 + 4096) >> 13; \
    /* 4471/8192 ~= (1/Sqrt[2] - Cos[23*Pi/64]/2)/Sin[23*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s4, 4471, 4096, 259); \
    sb += (s4*4471 + 4096) >> 13; \
    /* 1309/1024 ~= Sqrt[2]*Sin[23*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(sb, 1309, 512, 260); \
    s4 -= (sb*1309 + 512) >> 10; \
    /* 5067/16384 ~= (1/Sqrt[2] - Cos[23*Pi/64])/Sin[23*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s4, 5067, 8192, 261); \
    sb += (s4*5067 + 8192) >> 14; \
    /* 2217/4096 ~= (1/Sqrt[2] - Cos[11*Pi/64]/2)/Sin[11*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s5, 2217, 2048, 262); \
    sa -= (s5*2217 + 2048) >> 12; \
    /* 1489/2048 ~= Sqrt[2]*Sin[11*Pi/64] ~= 0.72705107329128 */ \
    OD_DCT_OVERFLOW_CHECK(sa, 1489, 1024, 263); \
    s5 += (sa*1489 + 1024) >> 11; \
    /* 75/256 ~= (1/Sqrt[2] - Cos[11*Pi/64])/Sin[11*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s5, 75, 128, 264); \
    sa += (s5*75 + 128) >> 8; \
    /* 2087/4096 ~= (1/Sqrt[2] - Cos[19*Pi/64]/2)/Sin[19*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s9, 2087, 2048, 265); \
    s6 -= (s9*2087 + 2048) >> 12; \
    /* 4653/4096 ~= Sqrt[2]*Sin[19*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s6, 4653, 2048, 266); \
    s9 += (s6*4653 + 2048) >> 12; \
    /* 4545/32768 ~= (1/Sqrt[2] - Cos[19*Pi/64])/Sin[19*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s9, 4545, 16384, 267); \
    s6 -= (s9*4545 + 16384) >> 15; \
    /* 2053/4096 ~= (1/Sqrt[2] - Cos[15*Pi/64]/2)/Sin[15*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s8, 2053, 2048, 268); \
    s7 += (s8*2053 + 2048) >> 12; \
    /* 1945/2048 ~= Sqrt[2]*Sin[15*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s7, 1945, 1024, 269); \
    s8 -= (s7*1945 + 1024) >> 11; \
    /* 1651/32768 ~= (1/Sqrt[2] - Cos[15*Pi/64])/Sin[15*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s8, 1651, 16384, 270); \
    s7 -= (s8*1651 + 16384) >> 15; \
  } \
  while (0)

#define OD_IDST_16(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-IV iDST. */ \
  do { \
    int s0h; \
    int s4h; \
    int sbh; \
    int sfh; \
    /* 1651/32768 ~= (1/Sqrt[2] - Cos[15*Pi/64])/Sin[15*Pi/64] */ \
    se += (s1*1651 + 16384) >> 15; \
    /* 1945/2048 ~= Sqrt[2]*Sin[15*Pi/64] */ \
    s1 += (se*1945 + 1024) >> 11; \
    /* 2053/4096 ~= (1/Sqrt[2] - Cos[15*Pi/64]/2)/Sin[15*Pi/64] */ \
    se -= (s1*2053 + 2048) >> 12; \
    /* 4545/32768 ~= (1/Sqrt[2] - Cos[19*Pi/64])/Sin[19*Pi/64] */ \
    s6 += (s9*4545 + 16384) >> 15; \
    /* 4653/32768 ~= Sqrt[2]*Sin[19*Pi/64] */ \
    s9 -= (s6*4653 + 2048) >> 12; \
    /* 2087/4096 ~= (1/Sqrt[2] - Cos[19*Pi/64]/2)/Sin[19*Pi/64] */ \
    s6 += (s9*2087 + 2048) >> 12; \
    /* 75/256 ~= (1/Sqrt[2] - Cos[11*Pi/64])/Sin[11*Pi/64] */ \
    s5 -= (sa*75 + 128) >> 8; \
    /* 1489/2048 ~= Sqrt[2]*Sin[11*Pi/64] */ \
    sa -= (s5*1489 + 1024) >> 11; \
    /* 2217/4096 ~= (1/Sqrt[2] - Cos[11*Pi/64]/2)/Sin[11*Pi/64] */ \
    s5 += (sa*2217 + 2048) >> 12; \
    /* 5067/16384 ~= (1/Sqrt[2] - Cos[23*Pi/64])/Sin[23*Pi/64] */ \
    sd -= (s2*5067 + 8192) >> 14; \
    /* 1309/1024 ~= Sqrt[2]*Sin[23*Pi/64] */ \
    s2 += (sd*1309 + 512) >> 10; \
    /* 4471/8192 ~= (1/Sqrt[2] - Cos[23*Pi/64]/2)/Sin[23*Pi/64] */ \
    sd -= (s2*4471 + 4096) >> 13; \
    /* 5701/8192 ~= (1/Sqrt[2] - Cos[7*Pi/64])/Sin[7*Pi/64] */  \
    s3 -= (sc*5701 + 4096) >> 13; \
    /* 3903/8192 ~= Sqrt[2]*Sin[7*Pi/64] */ \
    sc -= (s3*3903 + 4096) >> 13; \
    /* 5747/8192 ~= (1/Sqrt[2] - Cos[7*Pi/64]/2)/Sin[7*Pi/64] */ \
    s3 += (sc*5747 + 4096) >> 13; \
    /* 7839/16384 ~= (1/Sqrt[2] - Cos[27*Pi/64])/Sin[27*Pi/64] */ \
    sb -= (s4*7839 + 8192) >> 14; \
    /* 5619/4096 ~= Sqrt[2]*Sin[27*Pi/64] */ \
    s4 += (sb*5619 + 2048) >> 12; \
    /* 2473/4096 ~= (1/Sqrt[2] - Cos[27*Pi/64]/2)/Sin[27*Pi/64] */ \
    sb -= (s4*2473 + 2048) >> 12; \
    /* 4641/8192 ~= (1/Sqrt[2] - Cos[29*Pi/64])/Sin[29*Pi/64] */ \
    s7 -= (s8*4641 + 4096) >> 13; \
    /* 2865/2048 ~= Sqrt[2]*Sin[29*Pi/64] */ \
    s8 += (s7*2865 + 1024) >> 11; \
    /* 41/64 ~= (1/Sqrt[2] - Cos[29*Pi/64]/2)/Sin[29*Pi/64] */ \
    s7 -= (s8*41 + 32) >> 6; \
    /* 5397/8192 ~= (Cos[Pi/4] - Cos[31*Pi/64])/Sin[31*Pi/64] */ \
    s0 += (sf*5397 + 4096) >> 13; \
    /* 2893/2048 ~= Sqrt[2]*Sin[31*Pi/64] */ \
    sf -= (s0*2893 + 1024) >> 11; \
    /* 2799/4096 ~= (1/Sqrt[2] - Cos[31*Pi/64]/2)/Sin[31*Pi/64] */ \
    s0 += (sf*2799 + 2048) >> 12; \
    sd -= OD_DCT_RSHIFT(sc, 1); \
    sc += sd; \
    s3 += OD_DCT_RSHIFT(s2, 1); \
    s2 = s3 - s2; \
    sb += OD_DCT_RSHIFT(sa, 1); \
    sa -= sb; \
    s5 = OD_DCT_RSHIFT(s4, 1) - s5; \
    s4 -= s5; \
    s7 = OD_DCT_RSHIFT(s9, 1) - s7; \
    s9 -= s7; \
    s6 -= OD_DCT_RSHIFT(s8, 1); \
    s8 += s6; \
    se = OD_DCT_RSHIFT(sf, 1) - se; \
    sf -= se; \
    s0 += OD_DCT_RSHIFT(s1, 1); \
    s1 -= s0; \
    s5 -= s9; \
    s9 += OD_DCT_RSHIFT(s5, 1); \
    sa = s6 - sa; \
    s6 -= OD_DCT_RSHIFT(sa, 1); \
    se += s2; \
    s2 -= OD_DCT_RSHIFT(se, 1); \
    s1 = sd - s1; \
    sd -= OD_DCT_RSHIFT(s1, 1); \
    s0 += s3; \
    s0h = OD_DCT_RSHIFT(s0, 1); \
    s3 = s0h - s3; \
    sf += sc; \
    sfh = OD_DCT_RSHIFT(sf, 1); \
    sc -= sfh; \
    sb = s7 - sb; \
    sbh = OD_DCT_RSHIFT(sb, 1); \
    s7 -= sbh; \
    s4 -= s8; \
    s4h = OD_DCT_RSHIFT(s4, 1); \
    s8 += s4h; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    se -= (s1*3227 + 16384) >> 15; \
    /* 6393/32768 ~= Sin[Pi/16] ~= 0.19509032201612825 */ \
    s1 += (se*6393 + 16384) >> 15; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    se -= (s1*3227 + 16384) >> 15; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    s6 -= (s9*2485 + 4096) >> 13; \
    /* 4551/8192 ~= Sin[3*Pi/16] ~= 0.555570233019602 */ \
    s9 += (s6*4551 + 4096) >> 13; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    s6 -= (s9*2485 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    s5 -= (sa*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    sa += (s5*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32]) ~= 0.534511135950792 */ \
    s5 -= (sa*8757 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32]) ~= 0.820678790828660 */ \
    s2 -= (sd*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    sd += (s2*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    s2 -= (sd*6723 + 4096) >> 13; \
    s9 += OD_DCT_RSHIFT(se, 1); \
    se = s9 - se; \
    s6 += OD_DCT_RSHIFT(s1, 1); \
    s1 -= s6; \
    sd = OD_DCT_RSHIFT(sa, 1) - sd; \
    sa -= sd; \
    s2 += OD_DCT_RSHIFT(s5, 1); \
    s5 = s2 - s5; \
    s3 -= sbh; \
    sb += s3; \
    sc += s4h; \
    s4 = sc - s4; \
    s8 = s0h - s8; \
    s0 -= s8; \
    s7 = sfh - s7; \
    sf -= s7; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s6 -= (s9*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    s9 += (s6*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s6 -= (s9*13573 + 16384) >> 15; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s5 -= (sa*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    sa += (s5*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s5 -= (sa*13573 + 16384) >> 15; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    s3 -= (sc*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    sc += (s3*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    s3 -= (sc*3259 + 8192) >> 14; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    sb -= (s4*21895 + 16384) >> 15; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    s4 += (sb*15137 + 8192) >> 14; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    sb -= (s4*21895 + 16384) >> 15; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s8 -= (s7*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    s7 += (s8*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s8 -= (s7*13573 + 16384) >> 15; \
  } \
  while (0)

void od_bin_fdct4(od_coeff y[4], const od_coeff *x, int xstride) {
  int q0;
  int q1;
  int q2;
  int q3;
  q0 = x[0*xstride];
  q2 = x[1*xstride];
  q1 = x[2*xstride];
  q3 = x[3*xstride];
  OD_FDCT_4(q0, q2, q1, q3);
  y[0] = (od_coeff)q0;
  y[1] = (od_coeff)q1;
  y[2] = (od_coeff)q2;
  y[3] = (od_coeff)q3;
}

void od_bin_idct4(od_coeff *x, int xstride, const od_coeff y[4]) {
  int q0;
  int q1;
  int q2;
  int q3;
  q0 = y[0];
  q2 = y[1];
  q1 = y[2];
  q3 = y[3];
  OD_IDCT_4(q0, q2, q1, q3);
  x[0*xstride] = q0;
  x[1*xstride] = q1;
  x[2*xstride] = q2;
  x[3*xstride] = q3;
}

void od_bin_fdct8(od_coeff y[8], const od_coeff *x, int xstride) {
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  r0 = x[0*xstride];
  r4 = x[1*xstride];
  r2 = x[2*xstride];
  r6 = x[3*xstride];
  r1 = x[4*xstride];
  r5 = x[5*xstride];
  r3 = x[6*xstride];
  r7 = x[7*xstride];
  OD_FDCT_8(r0, r4, r2, r6, r1, r5, r3, r7);
  y[0] = (od_coeff)r0;
  y[1] = (od_coeff)r1;
  y[2] = (od_coeff)r2;
  y[3] = (od_coeff)r3;
  y[4] = (od_coeff)r4;
  y[5] = (od_coeff)r5;
  y[6] = (od_coeff)r6;
  y[7] = (od_coeff)r7;
}

void od_bin_idct8(od_coeff *x, int xstride, const od_coeff y[8]) {
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  r0 = y[0];
  r4 = y[1];
  r2 = y[2];
  r6 = y[3];
  r1 = y[4];
  r5 = y[5];
  r3 = y[6];
  r7 = y[7];
  OD_IDCT_8(r0, r4, r2, r6, r1, r5, r3, r7);
  x[0*xstride] = (od_coeff)r0;
  x[1*xstride] = (od_coeff)r1;
  x[2*xstride] = (od_coeff)r2;
  x[3*xstride] = (od_coeff)r3;
  x[4*xstride] = (od_coeff)r4;
  x[5*xstride] = (od_coeff)r5;
  x[6*xstride] = (od_coeff)r6;
  x[7*xstride] = (od_coeff)r7;
}

void od_bin_fdst8(od_coeff y[8], const od_coeff *x, int xstride) {
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  r0 = x[0*xstride];
  r4 = x[1*xstride];
  r2 = x[2*xstride];
  r6 = x[3*xstride];
  r1 = x[4*xstride];
  r5 = x[5*xstride];
  r3 = x[6*xstride];
  r7 = x[7*xstride];
  OD_FDST_8(r0, r4, r2, r6, r1, r5, r3, r7);
  y[0] = (od_coeff)r0;
  y[1] = (od_coeff)r1;
  y[2] = (od_coeff)r2;
  y[3] = (od_coeff)r3;
  y[4] = (od_coeff)r4;
  y[5] = (od_coeff)r5;
  y[6] = (od_coeff)r6;
  y[7] = (od_coeff)r7;
}

void od_bin_idst8(od_coeff *x, int xstride, const od_coeff y[8]) {
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  r0 = y[0];
  r4 = y[1];
  r2 = y[2];
  r6 = y[3];
  r1 = y[4];
  r5 = y[5];
  r3 = y[6];
  r7 = y[7];
  OD_IDST_8(r0, r4, r2, r6, r1, r5, r3, r7);
  x[0*xstride] = (od_coeff)r0;
  x[1*xstride] = (od_coeff)r1;
  x[2*xstride] = (od_coeff)r2;
  x[3*xstride] = (od_coeff)r3;
  x[4*xstride] = (od_coeff)r4;
  x[5*xstride] = (od_coeff)r5;
  x[6*xstride] = (od_coeff)r6;
  x[7*xstride] = (od_coeff)r7;
}

void od_bin_fdct16(od_coeff y[16], const od_coeff *x, int xstride) {
  int s0;
  int s1;
  int s2;
  int s3;
  int s4;
  int s5;
  int s6;
  int s7;
  int s8;
  int s9;
  int sa;
  int sb;
  int sc;
  int sd;
  int se;
  int sf;
  s0 = x[0*xstride];
  s8 = x[1*xstride];
  s4 = x[2*xstride];
  sc = x[3*xstride];
  s2 = x[4*xstride];
  sa = x[5*xstride];
  s6 = x[6*xstride];
  se = x[7*xstride];
  s1 = x[8*xstride];
  s9 = x[9*xstride];
  s5 = x[10*xstride];
  sd = x[11*xstride];
  s3 = x[12*xstride];
  sb = x[13*xstride];
  s7 = x[14*xstride];
  sf = x[15*xstride];
  OD_FDCT_16(s0, s8, s4, sc, s2, sa, s6, se, s1, s9, s5, sd, s3, sb, s7, sf);
  y[0] = (od_coeff)s0;
  y[1] = (od_coeff)s1;
  y[2] = (od_coeff)s2;
  y[3] = (od_coeff)s3;
  y[4] = (od_coeff)s4;
  y[5] = (od_coeff)s5;
  y[6] = (od_coeff)s6;
  y[7] = (od_coeff)s7;
  y[8] = (od_coeff)s8;
  y[9] = (od_coeff)s9;
  y[10] = (od_coeff)sa;
  y[11] = (od_coeff)sb;
  y[12] = (od_coeff)sc;
  y[13] = (od_coeff)sd;
  y[14] = (od_coeff)se;
  y[15] = (od_coeff)sf;
}

void od_bin_idct16(od_coeff *x, int xstride, const od_coeff y[16]) {
  int s0;
  int s1;
  int s2;
  int s3;
  int s4;
  int s5;
  int s6;
  int s7;
  int s8;
  int s9;
  int sa;
  int sb;
  int sc;
  int sd;
  int se;
  int sf;
  s0 = y[0];
  s8 = y[1];
  s4 = y[2];
  sc = y[3];
  s2 = y[4];
  sa = y[5];
  s6 = y[6];
  se = y[7];
  s1 = y[8];
  s9 = y[9];
  s5 = y[10];
  sd = y[11];
  s3 = y[12];
  sb = y[13];
  s7 = y[14];
  sf = y[15];
  OD_IDCT_16(s0, s8, s4, sc, s2, sa, s6, se, s1, s9, s5, sd, s3, sb, s7, sf);
  x[0*xstride] = (od_coeff)s0;
  x[1*xstride] = (od_coeff)s1;
  x[2*xstride] = (od_coeff)s2;
  x[3*xstride] = (od_coeff)s3;
  x[4*xstride] = (od_coeff)s4;
  x[5*xstride] = (od_coeff)s5;
  x[6*xstride] = (od_coeff)s6;
  x[7*xstride] = (od_coeff)s7;
  x[8*xstride] = (od_coeff)s8;
  x[9*xstride] = (od_coeff)s9;
  x[10*xstride] = (od_coeff)sa;
  x[11*xstride] = (od_coeff)sb;
  x[12*xstride] = (od_coeff)sc;
  x[13*xstride] = (od_coeff)sd;
  x[14*xstride] = (od_coeff)se;
  x[15*xstride] = (od_coeff)sf;
}

void od_bin_fdst16(od_coeff y[16], const od_coeff *x, int xstride) {
  int s0;
  int s1;
  int s2;
  int s3;
  int s4;
  int s5;
  int s6;
  int s7;
  int s8;
  int s9;
  int sa;
  int sb;
  int sc;
  int sd;
  int se;
  int sf;
  s0 = x[15*xstride];
  s8 = x[14*xstride];
  s4 = x[13*xstride];
  sc = x[12*xstride];
  s2 = x[11*xstride];
  sa = x[10*xstride];
  s6 = x[9*xstride];
  se = x[8*xstride];
  s1 = x[7*xstride];
  s9 = x[6*xstride];
  s5 = x[5*xstride];
  sd = x[4*xstride];
  s3 = x[3*xstride];
  sb = x[2*xstride];
  s7 = x[1*xstride];
  sf = x[0*xstride];
  OD_FDST_16(s0, s8, s4, sc, s2, sa, s6, se, s1, s9, s5, sd, s3, sb, s7, sf);
  y[0] = (od_coeff)sf;
  y[1] = (od_coeff)se;
  y[2] = (od_coeff)sd;
  y[3] = (od_coeff)sc;
  y[4] = (od_coeff)sb;
  y[5] = (od_coeff)sa;
  y[6] = (od_coeff)s9;
  y[7] = (od_coeff)s8;
  y[8] = (od_coeff)s7;
  y[9] = (od_coeff)s6;
  y[10] = (od_coeff)s5;
  y[11] = (od_coeff)s4;
  y[12] = (od_coeff)s3;
  y[13] = (od_coeff)s2;
  y[14] = (od_coeff)s1;
  y[15] = (od_coeff)s0;
}

void od_bin_idst16(od_coeff *x, int xstride, const od_coeff y[16]) {
  int s0;
  int s1;
  int s2;
  int s3;
  int s4;
  int s5;
  int s6;
  int s7;
  int s8;
  int s9;
  int sa;
  int sb;
  int sc;
  int sd;
  int se;
  int sf;
  s0 = y[15];
  s8 = y[14];
  s4 = y[13];
  sc = y[12];
  s2 = y[11];
  sa = y[10];
  s6 = y[9];
  se = y[8];
  s1 = y[7];
  s9 = y[6];
  s5 = y[5];
  sd = y[4];
  s3 = y[3];
  sb = y[2];
  s7 = y[1];
  sf = y[0];
  OD_IDST_16(s0, s8, s4, sc, s2, sa, s6, se, s1, s9, s5, sd, s3, sb, s7, sf);
  x[0*xstride] = (od_coeff)sf;
  x[1*xstride] = (od_coeff)se;
  x[2*xstride] = (od_coeff)sd;
  x[3*xstride] = (od_coeff)sc;
  x[4*xstride] = (od_coeff)sb;
  x[5*xstride] = (od_coeff)sa;
  x[6*xstride] = (od_coeff)s9;
  x[7*xstride] = (od_coeff)s8;
  x[8*xstride] = (od_coeff)s7;
  x[9*xstride] = (od_coeff)s6;
  x[10*xstride] = (od_coeff)s5;
  x[11*xstride] = (od_coeff)s4;
  x[12*xstride] = (od_coeff)s3;
  x[13*xstride] = (od_coeff)s2;
  x[14*xstride] = (od_coeff)s1;
  x[15*xstride] = (od_coeff)s0;
}
