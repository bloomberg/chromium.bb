// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_PONG_PONG_MODEL_H_
#define EXAMPLES_PONG_PONG_MODEL_H_

#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

struct BallModel;

enum MoveDirection {
  MOVE_NONE,
  MOVE_UP,
  MOVE_DOWN
};

enum PaddleCollision {
  PADDLE_COLLISION_NORMAL,
  PADDLE_COLLISION_SPIN_UP,
  PADDLE_COLLISION_SPIN_DOWN
};

struct PaddleModel {
  PaddleModel();

  /**
   * Set the position of the paddle, given a court.
   * @param[in] court The bounds to place the paddle in.
   * @param[in] x_fraction A value in the range [0, 1]. 0 is on the left, 1 is
   *     on the right of the court.
   * @param[in] y_fraction A value in the range [0, 1]. 0 is on the top, 1 is on
   *     the bottom of the court.
   */
  void SetPosition(const pp::Rect& court, float x_fraction, float y_fraction);

  /**
   * Move the paddle, given |dir|. The paddle will not move in if it will
   * collide with |ball| or exit |court| after moving.
   * @param[in] court The bounds to move the ball in.
   * @param[in] ball The ball to avoid colliding with.
   * @param[in] dir The direction to move the paddle.
   */
  void Move(const pp::Rect& court, const BallModel& ball, MoveDirection dir);

  /**
   * Given that a ball has collided with this paddle, determine if "spin"
   * should be applied to the ball.
   * @param[in] collision_y The y-coordinate of the ball that collided with the
   *     paddle.
   * @return The paddle collision type.
   */
  PaddleCollision GetPaddleCollision(float collision_y) const;

  pp::Rect rect;
};

struct BallModel {
  BallModel();

  /**
   * Set the position of the ball, given a court.
   * @param[in] court The bounds to place the ball in.
   * @param[in] x_fraction A value in the range [0, 1]. 0 is on the left, 1 is
   *     on the right of the court.
   * @param[in] y_fraction A value in the range [0, 1]. 0 is on the top, 1 is on
   *     the bottom of the court.
   */
  void SetPosition(const pp::Rect& court, float x_fraction, float y_fraction);

  /** Move the ball in its current direction.
   * @param[in] dt A time delta value in the range [0, 1]. The ball will move
   *     its update distance, multiplied by dt for each axis.
   *     For example, if dt==1, the ball will move its full update distance. If
   *     dt==0.5, it will move half its update distance, etc.
   */
  void Move(float dt);

  /** Given that the ball collided with the paddle, determine how it should
   * bounce off it, applying "spin". If the ball hits the top of the paddle it
   * will spin up, if the ball hits the bottom of the paddle it will spin down.
   * @param[in] collision The type of collision with the paddle.
   */
  void ApplyPaddleCollision(PaddleCollision collision);

  /** Calculate the time (as a fraction of the number of updates), until the
   * ball collides with |paddle|.
   * @param[in] paddle The paddle to test for collision with.
   * @return The time (maybe negative) when the ball will collide with the
   *    paddle.
   *    If the time is in the range [0, 1], the ball will collide with the
   *    paddle during this update.
   *    If the time is > 1, then the ball will collide with the paddle after
   *    ceil(time) updates, assuming the paddle doesn't move.
   *    Because we want to find the most recent collision (i.e. smallest time
   *    value), we use the value FLT_MAX to represent a collision that will
   *    never happen (the paddle is behind the ball).
   */
  float GetPaddleCollisionTime(const PaddleModel& paddle) const;

  /** Calculate the time (as a fraction of the number of updates), until the
   * ball collides with |court|.
   * @param[in] court The bounds of the court to test for collision with.
   * @return The time (maybe negative) when the ball will collide with bottom
   *     or top of the court.
   * @see GetPaddleCollisionTime to understand the meaning of the return time.
   */
  float GetCourtCollisionTime(const pp::Rect& court) const;

  pp::Rect rect;
  int dx;
  int dy;
};

class PongModelDelegate {
 public:
  /** Called when the score changes, whether due to PongModel::SetScore, or
   * when a player scores. */
  virtual void OnScoreChanged() = 0;
  /** Called only when a player scores, not when the score is changed
   * programmatically. */
  virtual void OnPlayerScored() = 0;
};

class PongModel {
 public:
  explicit PongModel(PongModelDelegate* delegate);

  void SetCourtSize(const pp::Size& size);
  void SetScore(int left_score, int right_score);
  void ResetPositions();
  void Update(MoveDirection left_movement, MoveDirection right_movement);

  const PaddleModel& left_paddle() const { return left_paddle_; }
  const PaddleModel& right_paddle() const { return right_paddle_; }
  const BallModel& ball() const { return ball_; }
  int left_score() const { return left_score_; }
  int right_score() const { return right_score_; }

 private:
  void UpdateBall();

  PongModelDelegate* delegate_;  // Weak pointer.
  PaddleModel left_paddle_;
  PaddleModel right_paddle_;
  BallModel ball_;
  pp::Rect court_;
  int left_score_;
  int right_score_;
};

#endif  // EXAMPLES_PONG_PONG_MODEL_H_
