#ifndef _GEARS_H_
#define _GEARS_H_

struct gears;

struct gears *gears_create(void);

void gears_draw(struct gears *gears, GLfloat angle);

#endif
